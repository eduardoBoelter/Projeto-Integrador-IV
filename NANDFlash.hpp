#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

// Tamanhos padrão configuráveis
constexpr size_t PAGE_SIZE = 2048;      // 2 KB por página
constexpr size_t PAGES_PER_BLOCK = 64;  // 64 páginas por bloco
constexpr size_t TOTAL_BLOCKS = 128;    // Total de blocos na mídia simulada
constexpr uint32_t MAX_PE_CYCLES = 1000; // Limite teórico de vida útil por bloco

constexpr uint32_t INVALID_LBA = 0xFFFFFFFF; // Página sem LBA associado
constexpr uint8_t ERASED_BYTE = 0xFF;        // Valor de um byte apagado na NAND

// Identificação do arquivo de metadados (nand_meta.bin)
constexpr char META_MAGIC[8] = {'N', 'A', 'N', 'D', 'M', 'E', 'T', 'A'};
constexpr uint32_t META_VERSION = 1;

// Estado de uma Página na Memória Flash
enum class PageState : uint8_t {
    FREE = 0,    // Página virgem, pronta para escrita
    VALID = 1,   // Página com dado atualizado e válido
    INVALID = 2  // Página com dado obsoleto/sobrescrito
};

// Resultado de uma operação de apagamento
enum class EraseResult : uint8_t {
    OK = 0,         // Bloco apagado e ainda utilizável
    WORN_OUT = 1,   // Bloco apagado, mas atingiu o limite de P/E e virou bad block
    BAD_BLOCK = 2,  // Bloco já era bad block, nada foi feito
    IO_ERROR = 3,   // Falha ao acessar o arquivo binário
    OUT_OF_RANGE = 4 // Bloco inexistente
};

// Resultado da abertura de um dispositivo já existente
enum class OpenResult : uint8_t {
    OK = 0,                 // Dados e metadados carregados
    NO_DEVICE = 1,          // Arquivo de dados ou de metadados não existe
    INVALID_FORMAT = 2,     // Metadados corrompidos ou de outra versão
    GEOMETRY_MISMATCH = 3,  // Arquivos criados com outros parâmetros (página, bloco...)
    IO_ERROR = 4            // Falha de leitura
};

// Metadados de uma Página Física (os dados ficam no arquivo binário)
struct Page {
    PageState state = PageState::FREE;
    uint32_t logical_address = INVALID_LBA; // LBA associado
};

// Metadados de um Bloco Físico
class NANDBlock {
public:
    uint32_t block_id;
    uint32_t pe_cycles = 0;             // Contador de ciclos de Programação/Apagamento (P/E)
    bool is_bad_block = false;          // Marcação de falha permanente de hardware
    size_t valid_pages_count = 0;       // Quantidade de páginas válidas
    size_t invalid_pages_count = 0;     // Quantidade de páginas inválidas
    size_t free_pages_count = PAGES_PER_BLOCK;

    std::vector<Page> pages;

    explicit NANDBlock(uint32_t id) : block_id(id), pages(PAGES_PER_BLOCK) {}

    // Reseta os metadados do bloco e contabiliza o ciclo P/E.
    // Deve ser chamado somente após o conteúdo físico ter sido apagado.
    EraseResult reset_after_erase() {
        pe_cycles++;

        for (auto& page : pages) {
            page.state = PageState::FREE;
            page.logical_address = INVALID_LBA;
        }

        valid_pages_count = 0;
        invalid_pages_count = 0;
        free_pages_count = PAGES_PER_BLOCK;

        if (pe_cycles >= MAX_PE_CYCLES) {
            is_bad_block = true; // Bloco queimado/inutilizável
            return EraseResult::WORN_OUT;
        }
        return EraseResult::OK;
    }

    // Recalcula os contadores de páginas a partir do estado de cada página.
    // Usado ao carregar os metadados do disco, que guardam só os estados.
    void recount_pages() {
        valid_pages_count = invalid_pages_count = free_pages_count = 0;
        for (const auto& page : pages) {
            switch (page.state) {
                case PageState::FREE:    free_pages_count++;    break;
                case PageState::VALID:   valid_pages_count++;   break;
                case PageState::INVALID: invalid_pages_count++; break;
            }
        }
    }

    // Volta o bloco ao estado de fábrica (usado na formatação)
    void reset_to_factory() {
        pe_cycles = 0;
        is_bad_block = false;
        for (auto& page : pages) page = Page{};
        recount_pages();
    }
};

// Classe principal de abstração do Hardware da Memória NAND.
// O conteúdo das páginas fica em data_filename (nand_device.bin) e os
// metadados (ciclos P/E, bad blocks, estado e LBA das páginas) ficam em
// meta_filename (nand_meta.bin), para o desgaste sobreviver entre execuções.
class NANDFlashMemory {
private:
    std::string data_filename;
    std::string meta_filename;

    // Arquivo de dados mantido aberto enquanto o dispositivo está em uso,
    // para não reabrir o arquivo a cada operação de página.
    std::fstream device;

    bool attach_device() {
        device.close();
        device.clear();
        device.open(data_filename, std::ios::binary | std::ios::in | std::ios::out);
        return device.is_open();
    }

    // Posição (em bytes) de uma página dentro do arquivo binário
    static std::streamoff page_offset(uint32_t block_id, uint32_t page_id) {
        return static_cast<std::streamoff>(
            (static_cast<size_t>(block_id) * PAGES_PER_BLOCK + page_id) * PAGE_SIZE);
    }

    static constexpr std::streamoff device_size() {
        return static_cast<std::streamoff>(TOTAL_BLOCKS * PAGES_PER_BLOCK * PAGE_SIZE);
    }

    // Inteiros gravados sempre em little-endian, para o arquivo de metadados
    // ser o mesmo em qualquer máquina (Windows ou Linux).
    static void write_u32(std::ostream& out, uint32_t value) {
        char bytes[4];
        for (int i = 0; i < 4; ++i) bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
        out.write(bytes, 4);
    }

    static bool read_u32(std::istream& in, uint32_t& value) {
        unsigned char bytes[4];
        if (!in.read(reinterpret_cast<char*>(bytes), 4)) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
        return true;
    }

    // Cria o arquivo de dados com todas as páginas apagadas (0xFF)
    bool create_data_file() {
        std::ofstream file(data_filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;

        std::vector<char> empty_block(PAGE_SIZE * PAGES_PER_BLOCK, static_cast<char>(ERASED_BYTE));
        for (size_t b = 0; b < TOTAL_BLOCKS; ++b) {
            file.write(empty_block.data(), static_cast<std::streamsize>(empty_block.size()));
        }
        return file.good();
    }

public:
    std::vector<NANDBlock> blocks;

    explicit NANDFlashMemory(const std::string& data_file = "nand_device.bin",
                             const std::string& meta_file = "nand_meta.bin")
        : data_filename(data_file), meta_filename(meta_file) {
        blocks.reserve(TOTAL_BLOCKS);
        for (uint32_t i = 0; i < TOTAL_BLOCKS; ++i) {
            blocks.emplace_back(i);
        }
    }

    // Formata o dispositivo: recria o arquivo de dados com 0xFF, zera todos os
    // contadores (como um chip novo de fábrica) e grava os metadados.
    bool format() {
        device.close();
        for (auto& block : blocks) block.reset_to_factory();
        return create_data_file() && attach_device() && save_metadata();
    }

    // Abre um dispositivo já existente, recarregando o desgaste acumulado.
    // Se a abertura falhar, o dispositivo que já estava em uso continua intacto.
    OpenResult open() {
        {
            std::ifstream data(data_filename, std::ios::binary | std::ios::ate);
            if (!data.is_open()) return OpenResult::NO_DEVICE;
            if (data.tellg() != device_size()) return OpenResult::GEOMETRY_MISMATCH;
        }
        OpenResult result = load_metadata();
        if (result == OpenResult::OK && !attach_device()) return OpenResult::IO_ERROR;
        return result;
    }

    bool is_open() const { return device.is_open(); }

    // Grava os metadados em nand_meta.bin.
    // Formato: cabeçalho (assinatura, versão e geometria) seguido, para cada
    // bloco, do contador P/E, da marca de bad block e do estado/LBA de cada página.
    // É chamado explicitamente (como um "flush"), e não a cada operação,
    // para não multiplicar o custo das simulações longas.
    bool save_metadata() const {
        std::ofstream out(meta_filename, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        out.write(META_MAGIC, sizeof(META_MAGIC));
        write_u32(out, META_VERSION);
        write_u32(out, static_cast<uint32_t>(PAGE_SIZE));
        write_u32(out, static_cast<uint32_t>(PAGES_PER_BLOCK));
        write_u32(out, static_cast<uint32_t>(TOTAL_BLOCKS));
        write_u32(out, MAX_PE_CYCLES);

        for (const auto& block : blocks) {
            write_u32(out, block.pe_cycles);
            out.put(block.is_bad_block ? 1 : 0);
            for (const auto& page : block.pages) {
                out.put(static_cast<char>(page.state));
                write_u32(out, page.logical_address);
            }
        }
        return out.good();
    }

    // Lê nand_meta.bin, validando assinatura, versão e geometria.
    // Só altera os blocos em memória se o arquivo inteiro for válido.
    OpenResult load_metadata() {
        std::ifstream in(meta_filename, std::ios::binary);
        if (!in.is_open()) return OpenResult::NO_DEVICE;

        char magic[sizeof(META_MAGIC)];
        uint32_t version, page_size, pages_per_block, total_blocks, max_pe;
        if (!in.read(magic, sizeof(magic)) || !read_u32(in, version))
            return OpenResult::INVALID_FORMAT;
        if (std::string(magic, sizeof(magic)) != std::string(META_MAGIC, sizeof(META_MAGIC)) ||
            version != META_VERSION)
            return OpenResult::INVALID_FORMAT;
        if (!read_u32(in, page_size) || !read_u32(in, pages_per_block) ||
            !read_u32(in, total_blocks) || !read_u32(in, max_pe))
            return OpenResult::INVALID_FORMAT;
        if (page_size != PAGE_SIZE || pages_per_block != PAGES_PER_BLOCK ||
            total_blocks != TOTAL_BLOCKS || max_pe != MAX_PE_CYCLES)
            return OpenResult::GEOMETRY_MISMATCH;

        std::vector<NANDBlock> loaded;
        loaded.reserve(TOTAL_BLOCKS);
        for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
            NANDBlock block(b);
            if (!read_u32(in, block.pe_cycles)) return OpenResult::INVALID_FORMAT;

            int bad = in.get();
            if (bad != 0 && bad != 1) return OpenResult::INVALID_FORMAT;
            block.is_bad_block = (bad == 1);

            for (auto& page : block.pages) {
                int state = in.get();
                if (state < 0 || state > static_cast<int>(PageState::INVALID))
                    return OpenResult::INVALID_FORMAT;
                page.state = static_cast<PageState>(state);
                if (!read_u32(in, page.logical_address)) return OpenResult::INVALID_FORMAT;
            }
            block.recount_pages();
            loaded.push_back(std::move(block));
        }
        // Sobrou conteúdo depois do último bloco: arquivo não corresponde ao esperado
        if (in.peek() != std::char_traits<char>::eof()) return OpenResult::INVALID_FORMAT;

        blocks = std::move(loaded);
        return OpenResult::OK;
    }

    // Apaga fisicamente um bloco: grava 0xFF em todas as suas páginas no arquivo
    // binário e atualiza os metadados (contador P/E e estado das páginas).
    EraseResult erase_block(uint32_t block_id) {
        if (block_id >= TOTAL_BLOCKS) return EraseResult::OUT_OF_RANGE;
        NANDBlock& block = blocks[block_id];
        if (block.is_bad_block) return EraseResult::BAD_BLOCK;

        if (!device.is_open()) return EraseResult::IO_ERROR;

        static const std::vector<char> empty_block(PAGE_SIZE * PAGES_PER_BLOCK,
                                                   static_cast<char>(ERASED_BYTE));
        device.seekp(page_offset(block_id, 0));
        device.write(empty_block.data(), static_cast<std::streamsize>(empty_block.size()));
        device.flush();
        if (!device.good()) {
            device.clear();
            return EraseResult::IO_ERROR;
        }

        return block.reset_after_erase();
    }

    // Operações "elétricas" de página: leem e gravam os 2 KB no arquivo, sem
    // nenhuma regra. As regras da Flash (não sobrescrever, não usar bad block,
    // atualizar estados) ficam no driver de E/S (IODriver.hpp).
    bool read_page_data(uint32_t block_id, uint32_t page_id, uint8_t* buffer) {
        if (!device.is_open()) return false;
        device.seekg(page_offset(block_id, page_id));
        device.read(reinterpret_cast<char*>(buffer), PAGE_SIZE);
        if (!device.good()) {
            device.clear();
            return false;
        }
        return true;
    }

    bool write_page_data(uint32_t block_id, uint32_t page_id, const uint8_t* data) {
        if (!device.is_open()) return false;
        device.seekp(page_offset(block_id, page_id));
        device.write(reinterpret_cast<const char*>(data), PAGE_SIZE);
        device.flush();
        if (!device.good()) {
            device.clear();
            return false;
        }
        return true;
    }

    // Resumo do desgaste, útil para o shell e para os testes
    uint64_t total_erases() const {
        uint64_t total = 0;
        for (const auto& block : blocks) total += block.pe_cycles;
        return total;
    }

    size_t bad_block_count() const {
        size_t count = 0;
        for (const auto& block : blocks) count += block.is_bad_block ? 1 : 0;
        return count;
    }

    size_t get_total_blocks() const { return TOTAL_BLOCKS; }
    const std::string& data_file() const { return data_filename; }
    const std::string& meta_file() const { return meta_filename; }
};
