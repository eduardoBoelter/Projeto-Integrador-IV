// Teste da FTL com mapeamento direto (Etapa 4).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cstdio>
#include <iostream>
#include <vector>
#include "../FTL.hpp"

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

static const char* DATA = "test_ftl_device.bin";
static const char* META = "test_ftl_meta.bin";

static std::vector<uint8_t> pattern(uint32_t seed) {
    std::vector<uint8_t> data(PAGE_SIZE);
    for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(seed * 31 + i);
    return data;
}

static void test_mapping() {
    std::cout << "Traducao LBA -> PBA" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    DirectFTL ftl(driver);

    CHECK(ftl.logical_capacity() == 7680, "capacidade logica de 7680 LBAs (120 blocos)");

    auto data = pattern(1);
    ftl.write(130, data.data()); // 130 = bloco 2, pagina 2
    CHECK(driver.page_info({2, 2}).state == PageState::VALID &&
          driver.page_info({2, 2}).logical_address == 130, "LBA 130 gravado no bloco 2, pagina 2");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    CHECK(ftl.read(130, buffer.data()) == FTLResult::OK && buffer == data, "le de volta o LBA 130");
    CHECK(ftl.read(131, buffer.data()) == FTLResult::OK && buffer[0] == ERASED_BYTE,
          "LBA nunca gravado retorna 0xFF");
    CHECK(ftl.write(LOGICAL_PAGES, data.data()) == FTLResult::OUT_OF_RANGE, "LBA alem da capacidade e recusado");
    CHECK(driver.stats().block_erases == 0, "primeiras gravacoes nao apagam nada");
}

static void test_rewrite() {
    std::cout << "Alteracao de um LBA (read-modify-erase-write)" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    DirectFTL ftl(driver);

    // Preenche o bloco 1 inteiro (LBAs 64 a 127)
    for (uint32_t lba = 64; lba < 128; ++lba) ftl.write(lba, pattern(lba).data());

    auto updated = pattern(999);
    CHECK(ftl.write(70, updated.data()) == FTLResult::OK, "altera o LBA 70");
    CHECK(driver.block_info(1).pe_cycles == 1, "o bloco 1 foi apagado uma vez");
    CHECK(driver.block_info(1).valid_pages_count == PAGES_PER_BLOCK, "bloco continua com 64 paginas validas");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    ftl.read(70, buffer.data());
    CHECK(buffer == updated, "LBA 70 tem o dado novo");

    bool others_ok = true;
    for (uint32_t lba = 64; lba < 128; ++lba) {
        if (lba == 70) continue;
        ftl.read(lba, buffer.data());
        others_ok = others_ok && (buffer == pattern(lba));
    }
    CHECK(others_ok, "os outros 63 LBAs do bloco foram preservados");
    CHECK(driver.stats().page_programs == 64 + 64, "alterar 1 pagina regravou as 64 do bloco");
}

static void test_partial_block() {
    std::cout << "Bloco parcialmente gravado" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    DirectFTL ftl(driver);

    ftl.write(0, pattern(1).data());
    ftl.write(5, pattern(5).data());
    ftl.write(0, pattern(2).data());
    CHECK(driver.block_info(0).valid_pages_count == 2 && driver.block_info(0).free_pages_count == PAGES_PER_BLOCK - 2,
          "paginas livres continuam livres apos a regravacao");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    ftl.read(5, buffer.data());
    CHECK(buffer == pattern(5), "LBA 5 preservado");
}

static void test_wear_out() {
    std::cout << "Fim de vida com dados quentes" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    DirectFTL ftl(driver);

    ftl.write(3, pattern(0).data());
    FTLResult result = FTLResult::OK;
    uint32_t updates = 0;
    while (result == FTLResult::OK) {
        result = ftl.write(3, pattern(++updates).data());
    }
    CHECK(result == FTLResult::DEVICE_WORN_OUT, "o LBA quente leva o dispositivo a DEVICE_WORN_OUT");
    CHECK(updates == MAX_PE_CYCLES, "falha exatamente na alteracao n. 1000");
    CHECK(ftl.stats().first_failure_write == MAX_PE_CYCLES + 1 && ftl.stats().first_failed_block == 0,
          "primeira falha registrada (escrita 1001, bloco 0)");
    CHECK(driver.block_info(1).pe_cycles == 0, "os outros blocos nao foram usados");
    CHECK(ftl.write(10, pattern(1).data()) == FTLResult::DEVICE_WORN_OUT,
          "outros LBAs do bloco morto tambem ficam inacessiveis");
    CHECK(ftl.write(64, pattern(1).data()) == FTLResult::OK, "LBAs de outros blocos continuam funcionando");
}

int main() {
    std::cout << "=== Teste da FTL com mapeamento direto ===" << std::endl;
    test_mapping();
    test_rewrite();
    test_partial_block();
    test_wear_out();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
