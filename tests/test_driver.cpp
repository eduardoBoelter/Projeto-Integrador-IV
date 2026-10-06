// Teste do driver de E/S (Etapa 3).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include "../IODriver.hpp"

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

static const char* DATA = "test_driver_device.bin";
static const char* META = "test_driver_meta.bin";

// Página preenchida com um padrão que depende de "seed", para comparar leituras
static std::vector<uint8_t> pattern(uint8_t seed) {
    std::vector<uint8_t> data(PAGE_SIZE);
    for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(seed + i * 7);
    return data;
}

static void test_read_write() {
    std::cout << "Leitura e escrita" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);

    std::vector<uint8_t> buffer(PAGE_SIZE);
    CHECK(driver.read_page({0, 0}, buffer.data()) == IOResult::OK, "le uma pagina livre");
    bool all_ff = true;
    for (uint8_t b : buffer) all_ff = all_ff && (b == ERASED_BYTE);
    CHECK(all_ff, "pagina livre contem apenas 0xFF");

    auto data = pattern(42);
    CHECK(driver.program_page({5, 3}, data.data(), 100) == IOResult::OK, "programa a pagina (5, 3)");
    CHECK(driver.read_page({5, 3}, buffer.data()) == IOResult::OK && buffer == data,
          "le de volta exatamente o que foi gravado");
    CHECK(driver.page_info({5, 3}).state == PageState::VALID &&
          driver.page_info({5, 3}).logical_address == 100, "pagina fica VALID com o LBA 100");
    CHECK(driver.read_page({5, 4}, buffer.data()) == IOResult::OK && buffer[0] == ERASED_BYTE,
          "pagina vizinha continua apagada");

    const NANDBlock& block = driver.block_info(5);
    CHECK(block.free_pages_count == PAGES_PER_BLOCK - 1 && block.valid_pages_count == 1,
          "contadores do bloco: 63 livres e 1 valida");
}

static void test_no_overwrite() {
    std::cout << "Proibicao de sobrescrita" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);

    auto first = pattern(1), second = pattern(2);
    driver.program_page({2, 0}, first.data(), 7);
    CHECK(driver.program_page({2, 0}, second.data(), 7) == IOResult::NOT_FREE,
          "regravar uma pagina valida retorna NOT_FREE");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    driver.read_page({2, 0}, buffer.data());
    CHECK(buffer == first, "conteudo original foi preservado");

    CHECK(driver.invalidate_page({2, 0}) == IOResult::OK, "invalida a pagina");
    CHECK(driver.program_page({2, 0}, second.data(), 7) == IOResult::NOT_FREE,
          "pagina invalida tambem nao pode ser regravada");

    CHECK(driver.erase_block(2) == EraseResult::OK, "apaga o bloco");
    CHECK(driver.program_page({2, 0}, second.data(), 7) == IOResult::OK,
          "apos o apagamento a pagina pode ser programada de novo");
    driver.read_page({2, 0}, buffer.data());
    CHECK(buffer == second, "novo conteudo gravado corretamente");
}

static void test_invalidate() {
    std::cout << "Invalidacao" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);

    auto data = pattern(9);
    CHECK(driver.invalidate_page({1, 0}) == IOResult::NOT_VALID, "nao invalida pagina livre");

    driver.program_page({1, 0}, data.data(), 10);
    driver.program_page({1, 1}, data.data(), 11);
    driver.invalidate_page({1, 0});
    const NANDBlock& block = driver.block_info(1);
    CHECK(block.valid_pages_count == 1 && block.invalid_pages_count == 1 &&
          block.free_pages_count == PAGES_PER_BLOCK - 2, "contadores: 1 valida, 1 invalida, 62 livres");
    CHECK(driver.invalidate_page({1, 0}) == IOResult::NOT_VALID, "nao invalida a mesma pagina duas vezes");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    driver.read_page({1, 0}, buffer.data());
    CHECK(buffer == data, "dado invalido continua no arquivo ate o apagamento");
}

static void test_bad_block_and_range() {
    std::cout << "Bad block e enderecos invalidos" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    auto data = pattern(3);

    while (driver.erase_block(4) == EraseResult::OK) {}
    CHECK(driver.block_info(4).is_bad_block, "bloco 4 desgastado ate virar bad block");
    CHECK(driver.program_page({4, 0}, data.data(), 1) == IOResult::BAD_BLOCK, "nao programa em bad block");
    CHECK(driver.erase_block(4) == EraseResult::BAD_BLOCK, "nao apaga bad block");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    CHECK(driver.read_page({TOTAL_BLOCKS, 0}, buffer.data()) == IOResult::OUT_OF_RANGE, "bloco inexistente");
    CHECK(driver.program_page({0, PAGES_PER_BLOCK}, data.data(), 1) == IOResult::OUT_OF_RANGE,
          "pagina inexistente");
    CHECK(driver.erase_block(TOTAL_BLOCKS) == EraseResult::OUT_OF_RANGE, "apagamento de bloco inexistente");
}

static void test_stats_and_persistence() {
    std::cout << "Estatisticas e persistencia" << std::endl;
    {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        auto data = pattern(5);
        std::vector<uint8_t> buffer(PAGE_SIZE);

        driver.program_page({0, 0}, data.data(), 0);
        driver.program_page({0, 1}, data.data(), 1);
        driver.program_page({0, 1}, data.data(), 1); // recusada
        driver.read_page({0, 0}, buffer.data());
        driver.invalidate_page({0, 0});
        driver.erase_block(3);

        const IOStats& s = driver.stats();
        CHECK(s.page_programs == 2 && s.page_reads == 1 && s.page_invalidations == 1 &&
              s.block_erases == 1 && s.rejected_operations == 1, "contadores de operacoes corretos");

        driver.program_page({0, 2}, data.data(), 2);
        flash.save_metadata();
    }
    NANDFlashMemory reopened(DATA, META);
    reopened.open();
    IODriver driver(reopened);
    CHECK(driver.page_info({0, 0}).state == PageState::INVALID &&
          driver.page_info({0, 2}).state == PageState::VALID &&
          driver.page_info({0, 2}).logical_address == 2, "estados gravados pelo driver sobrevivem a reabertura");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    driver.read_page({0, 2}, buffer.data());
    CHECK(buffer == pattern(5), "dados gravados pelo driver sobrevivem a reabertura");
}

int main() {
    std::cout << "=== Teste do driver de E/S ===" << std::endl;
    test_read_write();
    test_no_overwrite();
    test_invalidate();
    test_bad_block_and_range();
    test_stats_and_persistence();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
