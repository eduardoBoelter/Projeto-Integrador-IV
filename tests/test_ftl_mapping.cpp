// Teste da FTL com mapeamento de páginas e garbage collection (Etapa 5).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cstdio>
#include <iostream>
#include <random>
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

static const char* DATA = "test_map_device.bin";
static const char* META = "test_map_meta.bin";

static std::vector<uint8_t> pattern(uint32_t lba, uint32_t version) {
    std::vector<uint8_t> data(PAGE_SIZE);
    for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(lba * 7 + version * 13 + i);
    return data;
}

static void test_out_of_place() {
    std::cout << "Escrita fora do lugar" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);

    CHECK(ftl.free_blocks() == TOTAL_BLOCKS, "dispositivo novo: 128 blocos livres");

    ftl.write(500, pattern(500, 1).data());
    PhysicalAddress first{0, 0}, second{0, 0};
    CHECK(ftl.lookup(500, first), "LBA 500 mapeado apos a primeira escrita");
    CHECK(first.block == 0 && first.page == 0, "LBA 500 foi para a primeira pagina livre (nao para o bloco 7)");

    ftl.write(500, pattern(500, 2).data());
    ftl.lookup(500, second);
    CHECK(second.block == 0 && second.page == 1, "a alteracao foi para a pagina seguinte");
    CHECK(driver.page_info(first).state == PageState::INVALID, "a versao antiga ficou INVALIDA");
    CHECK(driver.stats().block_erases == 0, "nenhum apagamento para alterar o dado");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    CHECK(ftl.read(500, buffer.data()) == FTLResult::OK && buffer == pattern(500, 2), "leitura retorna a versao nova");
    ftl.read(501, buffer.data());
    CHECK(buffer[0] == ERASED_BYTE, "LBA nunca gravado retorna 0xFF");
    CHECK(ftl.write(LOGICAL_PAGES, buffer.data()) == FTLResult::OUT_OF_RANGE, "LBA alem da capacidade e recusado");
}

static void test_garbage_collection() {
    std::cout << "Garbage collection" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);

    // Enche o disco e depois reescreve LBAs aleatórios: força o GC a trabalhar
    std::vector<uint32_t> version(LOGICAL_PAGES, 0);
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) ftl.write(lba, pattern(lba, 0).data());
    CHECK(ftl.gc().runs == 0, "encher o disco nao aciona o GC (ha blocos reservados)");

    std::mt19937 rng(7);
    std::uniform_int_distribution<uint32_t> any_lba(0, LOGICAL_PAGES - 1);
    bool all_ok = true;
    for (int i = 0; i < 20000; ++i) {
        uint32_t lba = any_lba(rng);
        all_ok = all_ok && ftl.write(lba, pattern(lba, ++version[lba]).data()) == FTLResult::OK;
    }
    CHECK(all_ok, "20000 alteracoes aleatorias com o disco cheio, todas OK");
    CHECK(ftl.gc().runs > 0 && driver.stats().block_erases == ftl.gc().runs, "o GC foi acionado e e o unico que apaga blocos");
    CHECK(ftl.free_blocks() >= 1, "sempre sobra pelo menos 1 bloco livre");

    bool data_ok = true;
    std::vector<uint8_t> buffer(PAGE_SIZE);
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) {
        ftl.read(lba, buffer.data());
        data_ok = data_ok && (buffer == pattern(lba, version[lba]));
    }
    CHECK(data_ok, "os 7680 LBAs tem a versao mais recente apos o GC");

    size_t valid = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) valid += driver.block_info(b).valid_pages_count;
    CHECK(valid == LOGICAL_PAGES, "existe exatamente uma pagina valida por LBA");
    CHECK(driver.stats().page_programs == ftl.stats().host_writes + ftl.gc().pages_copied,
          "paginas programadas = escritas logicas + copias do GC");
}

static void test_wear_out_keeps_data() {
    std::cout << "Falha de bloco sem perda de dados" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);

    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) ftl.write(lba, pattern(lba, 0).data());
    uint32_t hot_version = 0;
    FTLResult result = FTLResult::OK;
    while (result == FTLResult::OK && ftl.stats().first_failure_write == 0) {
        result = ftl.write(0, pattern(0, ++hot_version).data());
    }
    CHECK(result == FTLResult::OK, "a escrita que causou a falha de bloco foi concluida");
    CHECK(ftl.stats().first_failure_write > 0, "primeira falha registrada");
    CHECK(driver.block_info(ftl.stats().first_failed_block).is_bad_block, "o bloco que falhou virou bad block");

    std::vector<uint8_t> buffer(PAGE_SIZE);
    ftl.read(0, buffer.data());
    bool ok = buffer == pattern(0, hot_version);
    for (uint32_t lba = 1; lba < LOGICAL_PAGES && ok; ++lba) {
        ftl.read(lba, buffer.data());
        ok = buffer == pattern(lba, 0);
    }
    CHECK(ok, "nenhum dado foi perdido: o GC copiou tudo antes do apagamento final");
    CHECK(ftl.write(1, pattern(1, 1).data()) == FTLResult::OK, "o dispositivo continua funcionando com 1 bloco a menos");
}

static void test_rebuild() {
    std::cout << "Reconstrucao do mapeamento ao reabrir" << std::endl;
    {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        PageMappingFTL ftl(driver);
        ftl.write(10, pattern(10, 1).data());
        ftl.write(20, pattern(20, 1).data());
        ftl.write(10, pattern(10, 2).data());
        flash.save_metadata();
    }
    NANDFlashMemory flash(DATA, META);
    flash.open();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);

    std::vector<uint8_t> buffer(PAGE_SIZE);
    ftl.read(10, buffer.data());
    bool ok10 = buffer == pattern(10, 2);
    ftl.read(20, buffer.data());
    CHECK(ok10 && buffer == pattern(20, 1), "LBAs 10 e 20 encontrados com a versao certa apos reabrir");
    CHECK(ftl.free_blocks() == TOTAL_BLOCKS - 1, "o bloco ja usado nao volta para a lista de livres");
    CHECK(ftl.write(30, pattern(30, 1).data()) == FTLResult::OK, "continua gravando normalmente");
    PhysicalAddress pba{0, 0};
    ftl.lookup(30, pba);
    CHECK(pba.block == 0 && pba.page == 3, "aproveita as paginas livres do bloco parcialmente gravado");
}

int main() {
    std::cout << "=== Teste da FTL com mapeamento de paginas ===" << std::endl;
    test_out_of_place();
    test_garbage_collection();
    test_wear_out_keeps_data();
    test_rebuild();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
