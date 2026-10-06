#pragma once

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

constexpr uint32_t INVALID_LBA = 0xFFFFFFFF; // Página sem LBA associado
constexpr uint8_t ERASED_BYTE = 0xFF;        // Valor de um byte apagado na NAND

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
    IO_ERROR = 3    // Falha ao acessar o arquivo binário
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
};

// Classe principal de abstração do Hardware da Memória NAND
class NANDFlashMemory {
private:
    std::string storage_filename;

    // Posição (em bytes) de uma página dentro do arquivo binário
    static std::streamoff page_offset(uint32_t block_id, uint32_t page_id) {
        return static_cast<std::streamoff>(
            (static_cast<size_t>(block_id) * PAGES_PER_BLOCK + page_id) * PAGE_SIZE);
    }

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
        std::vector<char> empty_page(PAGE_SIZE, static_cast<char>(ERASED_BYTE));
        for (size_t b = 0; b < TOTAL_BLOCKS; ++b) {
            for (size_t p = 0; p < PAGES_PER_BLOCK; ++p) {
                file.write(empty_page.data(), PAGE_SIZE);
            }
        }
        return file.good();
    }

    // Apaga fisicamente um bloco: grava 0xFF em todas as suas páginas no arquivo
    // binário e atualiza os metadados (contador P/E e estado das páginas).
    EraseResult erase_block(uint32_t block_id) {
        NANDBlock& block = blocks.at(block_id);
        if (block.is_bad_block) return EraseResult::BAD_BLOCK;

        std::fstream file(storage_filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return EraseResult::IO_ERROR;

        std::vector<char> empty_block(PAGE_SIZE * PAGES_PER_BLOCK, static_cast<char>(ERASED_BYTE));
        file.seekp(page_offset(block_id, 0));
        file.write(empty_block.data(), static_cast<std::streamsize>(empty_block.size()));
        if (!file.good()) return EraseResult::IO_ERROR;

        return block.reset_after_erase();
    }

    size_t get_total_blocks() const { return TOTAL_BLOCKS; }
};
