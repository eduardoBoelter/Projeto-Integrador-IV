// Teste da camada física (Etapa 2).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
#include "../NANDFlash.hpp"

static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (cond) {                                                   \
            std::cout << "  [OK]     " << msg << std::endl;           \
        } else {                                                      \
            std::cout << "  [FALHOU] " << msg << std::endl;           \
            failures++;                                               \
        }                                                             \
    } while (0)

static const char* DATA = "test_device.bin";
static const char* META = "test_meta.bin";

// Lê um byte do arquivo de dados numa posição qualquer
static int read_byte(std::streamoff offset) {
    std::ifstream in(DATA, std::ios::binary);
    in.seekg(offset);
    return in.get();
}

// Grava bytes "sujos" (0x00) numa página, simulando um dado gravado
static void dirty_page(uint32_t block, uint32_t page) {
    std::fstream f(DATA, std::ios::binary | std::ios::in | std::ios::out);
    std::vector<char> zeros(PAGE_SIZE, 0);
    f.seekp(static_cast<std::streamoff>((block * PAGES_PER_BLOCK + page) * PAGE_SIZE));
    f.write(zeros.data(), PAGE_SIZE);
}

static void test_format() {
    std::cout << "Formatacao" << std::endl;
    NANDFlashMemory flash(DATA, META);
    CHECK(flash.format(), "format() cria os arquivos");

    std::ifstream in(DATA, std::ios::binary | std::ios::ate);
    CHECK(in.tellg() == static_cast<std::streamoff>(TOTAL_BLOCKS * PAGES_PER_BLOCK * PAGE_SIZE),
          "arquivo de dados tem 16 MB");
    CHECK(read_byte(0) == ERASED_BYTE && read_byte(in.tellg() - std::streamoff(1)) == ERASED_BYTE,
          "conteudo inicial e 0xFF (apagado)");
    CHECK(flash.total_erases() == 0 && flash.bad_block_count() == 0, "nenhum desgaste apos formatar");
    CHECK(flash.blocks[5].free_pages_count == PAGES_PER_BLOCK, "todas as paginas comecam livres");
}

static void test_erase() {
    std::cout << "Apagamento" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();

    const uint32_t block = 3;
    dirty_page(block, 10);
    std::streamoff offset = static_cast<std::streamoff>((block * PAGES_PER_BLOCK + 10) * PAGE_SIZE);
    CHECK(read_byte(offset) == 0x00, "pagina foi 'suja' antes do teste");

    CHECK(flash.erase_block(block) == EraseResult::OK, "erase_block() retorna OK");
    CHECK(read_byte(offset) == ERASED_BYTE, "erase_block() grava 0xFF no arquivo");
    CHECK(flash.blocks[block].pe_cycles == 1, "contador P/E incrementado");
    CHECK(flash.blocks[block + 1].pe_cycles == 0, "bloco vizinho nao foi afetado");
}

static void test_wear_out() {
    std::cout << "Fim de vida do bloco" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();

    EraseResult result = EraseResult::OK;
    uint32_t erases = 0;
    while (result == EraseResult::OK) {
        result = flash.erase_block(7);
        erases++;
    }
    CHECK(result == EraseResult::WORN_OUT, "ultimo apagamento retorna WORN_OUT");
    CHECK(erases == MAX_PE_CYCLES, "bloco morre exatamente no ciclo MAX_PE_CYCLES");
    CHECK(flash.blocks[7].is_bad_block, "bloco marcado como bad block");
    CHECK(flash.erase_block(7) == EraseResult::BAD_BLOCK, "novo apagamento e recusado");
    CHECK(flash.blocks[7].pe_cycles == MAX_PE_CYCLES, "contador nao passa do limite");
}

static void test_persistence() {
    std::cout << "Persistencia dos metadados" << std::endl;
    {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        for (int i = 0; i < 5; ++i) flash.erase_block(2);
        flash.blocks[9].is_bad_block = true;
        flash.blocks[4].pages[0] = {PageState::VALID, 1234};
        flash.blocks[4].pages[1] = {PageState::INVALID, 99};
        CHECK(flash.save_metadata(), "save_metadata() grava o arquivo");
    }
    NANDFlashMemory reopened(DATA, META);
    CHECK(reopened.open() == OpenResult::OK, "open() carrega um dispositivo existente");
    CHECK(reopened.blocks[2].pe_cycles == 5, "contador P/E recuperado");
    CHECK(reopened.blocks[9].is_bad_block, "marca de bad block recuperada");
    CHECK(reopened.blocks[4].pages[0].state == PageState::VALID &&
          reopened.blocks[4].pages[0].logical_address == 1234, "estado e LBA da pagina recuperados");
    CHECK(reopened.blocks[4].valid_pages_count == 1 && reopened.blocks[4].invalid_pages_count == 1 &&
          reopened.blocks[4].free_pages_count == PAGES_PER_BLOCK - 2,
          "contadores de paginas recalculados");
}

static void test_open_errors() {
    std::cout << "Erros ao abrir" << std::endl;
    std::remove(DATA);
    std::remove(META);
    NANDFlashMemory flash(DATA, META);
    CHECK(flash.open() == OpenResult::NO_DEVICE, "sem arquivos: NO_DEVICE");

    flash.format();
    { std::ofstream(META, std::ios::binary | std::ios::trunc) << "LIXO"; }
    CHECK(flash.open() == OpenResult::INVALID_FORMAT, "metadados corrompidos: INVALID_FORMAT");

    flash.format();
    { std::ofstream(DATA, std::ios::binary | std::ios::app) << "x"; }
    CHECK(flash.open() == OpenResult::GEOMETRY_MISMATCH, "arquivo de dados com tamanho errado: GEOMETRY_MISMATCH");

    flash.format();
    flash.erase_block(1);
    { std::ofstream(META, std::ios::binary | std::ios::trunc) << "LIXO"; }
    flash.open();
    CHECK(flash.blocks[1].pe_cycles == 1, "falha ao carregar nao altera o estado em memoria");
}

int main() {
    std::cout << "=== Teste da camada fisica NAND ===" << std::endl;
    test_format();
    test_erase();
    test_wear_out();
    test_persistence();
    test_open_errors();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
