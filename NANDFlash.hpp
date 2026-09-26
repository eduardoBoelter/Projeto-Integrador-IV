#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include <string>

// Tamanhos padrão configuráveis
constexpr size_t PAGE_SIZE = 2048;      // 2 KB por página
constexpr size_t PAGES_PER_BLOCK = 64;  // 64 páginas por bloco
constexpr size_t TOTAL_BLOCKS = 128;    // Total de blocos na mídia simulada
constexpr uint32_t MAX_PE_CYCLES = 1000; // Limite teórico de vida útil por bloco

// Estado de uma Página na Memória Flash
enum class PageState : uint8_t {
    FREE = 0,    // Página virgem, pronta para escrita
    VALID = 1,   // Página com dado atualizado e válido
    INVALID = 2  // Página com dado obsoleto/sobrescrito
};

// Estrutura de uma Página Física
struct Page {
    PageState state = PageState::FREE;
    uint32_t logical_address = 0xFFFFFFFF; // LBA associado (0xFFFFFFFF = nenhum)
    std::vector<uint8_t> data = std::vector<uint8_t>(PAGE_SIZE, 0xFF);
};

// Estrutura de um Bloco Físico
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

    // Reseta as páginas do bloco para o estado virgem (Operação de Apagamento Físico)
    bool erase() {
        if (is_bad_block) return false;

        pe_cycles++;
        if (pe_cycles >= MAX_PE_CYCLES) {
            is_bad_block = true; // Bloco queimado/inutilizável
        }

        for (auto& page : pages) {
            page.state = PageState::FREE;
            page.logical_address = 0xFFFFFFFF;
            std::fill(page.data.begin(), page.data.end(), 0xFF);
        }

        valid_pages_count = 0;
        invalid_pages_count = 0;
        free_pages_count = PAGES_PER_BLOCK;
        return !is_bad_block;
    }
};

// Classe principal de abstração do Hardware da Memória NAND
class NANDFlashMemory {
private:
    std::string storage_filename;

public:
    std::vector<NANDBlock> blocks;

    explicit NANDFlashMemory(const std::string& filename) : storage_filename(filename) {
        blocks.reserve(TOTAL_BLOCKS);
        for (uint32_t i = 0; i < TOTAL_BLOCKS; ++i) {
            blocks.emplace_back(i);
        }
    }

    // Inicializa ou recria o arquivo binário que simula a persistência em disco
    bool initialize_storage() {
        std::ofstream file(storage_filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;

        // Preenche o arquivo binário simulando o tamanho total do dispositivo
        std::vector<char> empty_page(PAGE_SIZE, static_cast<char>(0xFF));
        for (size_t b = 0; b < TOTAL_BLOCKS; ++b) {
            for (size_t p = 0; p < PAGES_PER_BLOCK; ++p) {
                file.write(empty_page.data(), PAGE_SIZE);
            }
        }
        return true;
    }

    size_t get_total_blocks() const { return TOTAL_BLOCKS; }
};
