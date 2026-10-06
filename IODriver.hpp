#pragma once

#include <cstdint>
#include "NANDFlash.hpp"

// Endereço físico de uma página (PBA): bloco + página dentro do bloco
struct PhysicalAddress {
    uint32_t block;
    uint32_t page;
};

// Resultado das operações de página do driver
enum class IOResult : uint8_t {
    OK = 0,
    OUT_OF_RANGE = 1,  // Bloco ou página inexistente
    BAD_BLOCK = 2,     // Bloco já está inutilizado
    NOT_FREE = 3,      // Tentativa de programar uma página já escrita (sobrescrita proibida)
    NOT_VALID = 4,     // Tentativa de invalidar uma página que não está válida
    IO_ERROR = 5       // Falha ao acessar o arquivo binário
};

// Contadores de operações físicas. Servem de base para as métricas
// (por exemplo, a amplificação de escrita na Etapa 9).
struct IOStats {
    uint64_t page_reads = 0;
    uint64_t page_programs = 0;
    uint64_t page_invalidations = 0;
    uint64_t block_erases = 0;
    uint64_t rejected_operations = 0; // operações recusadas pelas regras da Flash
};

// Driver de E/S (Etapa 3).
// É a única porta de acesso da FTL à memória. Aplica as regras físicas da
// Flash NAND sobre as operações cruas da camada física:
//   - a leitura e a programação são feitas por página;
//   - uma página só pode ser programada se estiver livre (não há sobrescrita);
//   - o apagamento é feito por bloco inteiro;
//   - bad blocks não aceitam programação nem apagamento.
// Também mantém os estados e contadores de páginas de cada bloco.
class IODriver {
private:
    NANDFlashMemory& flash;
    IOStats io_stats;

    static bool in_range(PhysicalAddress pba) {
        return pba.block < TOTAL_BLOCKS && pba.page < PAGES_PER_BLOCK;
    }

    IOResult reject(IOResult reason) {
        io_stats.rejected_operations++;
        return reason;
    }

public:
    explicit IODriver(NANDFlashMemory& memory) : flash(memory) {}

    // Lê os 2 KB de uma página. Uma página livre retorna tudo em 0xFF.
    IOResult read_page(PhysicalAddress pba, uint8_t* buffer) {
        if (!in_range(pba)) return reject(IOResult::OUT_OF_RANGE);
        if (!flash.read_page_data(pba.block, pba.page, buffer)) return IOResult::IO_ERROR;
        io_stats.page_reads++;
        return IOResult::OK;
    }

    // Programa (grava) uma página livre e associa a ela o LBA informado.
    IOResult program_page(PhysicalAddress pba, const uint8_t* data, uint32_t lba) {
        if (!in_range(pba)) return reject(IOResult::OUT_OF_RANGE);

        NANDBlock& block = flash.blocks[pba.block];
        if (block.is_bad_block) return reject(IOResult::BAD_BLOCK);

        Page& page = block.pages[pba.page];
        if (page.state != PageState::FREE) return reject(IOResult::NOT_FREE);

        if (!flash.write_page_data(pba.block, pba.page, data)) return IOResult::IO_ERROR;

        page.state = PageState::VALID;
        page.logical_address = lba;
        block.free_pages_count--;
        block.valid_pages_count++;
        io_stats.page_programs++;
        return IOResult::OK;
    }

    // Marca uma página válida como inválida (dado obsoleto). Não toca no
    // arquivo: na Flash real, o dado antigo continua lá até o bloco ser apagado.
    IOResult invalidate_page(PhysicalAddress pba) {
        if (!in_range(pba)) return reject(IOResult::OUT_OF_RANGE);

        NANDBlock& block = flash.blocks[pba.block];
        Page& page = block.pages[pba.page];
        if (page.state != PageState::VALID) return reject(IOResult::NOT_VALID);

        page.state = PageState::INVALID;
        block.valid_pages_count--;
        block.invalid_pages_count++;
        io_stats.page_invalidations++;
        return IOResult::OK;
    }

    // Apaga um bloco inteiro (todas as páginas voltam a ficar livres).
    EraseResult erase_block(uint32_t block_id) {
        EraseResult result = flash.erase_block(block_id);
        if (result == EraseResult::OK || result == EraseResult::WORN_OUT) {
            io_stats.block_erases++;
        } else if (result == EraseResult::BAD_BLOCK || result == EraseResult::OUT_OF_RANGE) {
            io_stats.rejected_operations++;
        }
        return result;
    }

    // Consulta (somente leitura) dos metadados, usada pela FTL e pelas métricas
    const NANDBlock& block_info(uint32_t block_id) const { return flash.blocks.at(block_id); }
    const Page& page_info(PhysicalAddress pba) const {
        return flash.blocks.at(pba.block).pages.at(pba.page);
    }

    const IOStats& stats() const { return io_stats; }
    void reset_stats() { io_stats = IOStats{}; }
};

// Texto de cada resultado, para mensagens do simulador e dos testes
inline const char* to_string(IOResult result) {
    switch (result) {
        case IOResult::OK:           return "OK";
        case IOResult::OUT_OF_RANGE: return "OUT_OF_RANGE";
        case IOResult::BAD_BLOCK:    return "BAD_BLOCK";
        case IOResult::NOT_FREE:     return "NOT_FREE";
        case IOResult::NOT_VALID:    return "NOT_VALID";
        case IOResult::IO_ERROR:     return "IO_ERROR";
    }
    return "?";
}
