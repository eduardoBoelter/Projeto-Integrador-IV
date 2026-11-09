// Teste da FTL com wear leveling dinâmico e estático (Etapa 6).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
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

static const char* DATA = "test_wl_device.bin";
static const char* META = "test_wl_meta.bin";

static std::vector<uint8_t> pattern(uint32_t lba, uint32_t version) {
    std::vector<uint8_t> data(PAGE_SIZE);
    for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(lba * 7 + version * 13 + i);
    return data;
}

// Desvio padrão dos ciclos P/E de todos os blocos
static double pe_stdev(const IODriver& driver) {
    double mean = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) mean += driver.block_info(b).pe_cycles;
    mean /= TOTAL_BLOCKS;
    double var = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
        double d = driver.block_info(b).pe_cycles - mean;
        var += d * d;
    }
    return std::sqrt(var / TOTAL_BLOCKS);
}

static uint32_t pe_max(const IODriver& driver) {
    uint32_t m = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) m = std::max(m, driver.block_info(b).pe_cycles);
    return m;
}

// Carga de dados quentes: enche o disco e faz `updates` alterações,
// 90% nos LBAs 0 a 3 e 10% em LBAs aleatórios. Devolve a versão de cada LBA.
static std::vector<uint32_t> hot_workload(FTL& ftl, int updates, bool& all_ok) {
    std::vector<uint32_t> version(LOGICAL_PAGES, 0);
    all_ok = true;
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba)
        all_ok = all_ok && ftl.write(lba, pattern(lba, 0).data()) == FTLResult::OK;
    std::mt19937 rng(2026);
    std::uniform_int_distribution<uint32_t> cold(PAGES_PER_BLOCK, LOGICAL_PAGES - 1);
    std::uniform_int_distribution<int> percent(0, 99);
    for (int i = 0; i < updates; ++i) {
        uint32_t lba = percent(rng) < 90 ? static_cast<uint32_t>(i % 4) : cold(rng);
        all_ok = all_ok && ftl.write(lba, pattern(lba, ++version[lba]).data()) == FTLResult::OK;
    }
    return version;
}

static bool all_data_ok(FTL& ftl, const std::vector<uint32_t>& version) {
    std::vector<uint8_t> buffer(PAGE_SIZE);
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) {
        if (ftl.read(lba, buffer.data()) != FTLResult::OK || buffer != pattern(lba, version[lba])) return false;
    }
    return true;
}

static size_t total_valid(const IODriver& driver) {
    size_t valid = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) valid += driver.block_info(b).valid_pages_count;
    return valid;
}

static void test_dynamic_choice() {
    std::cout << "Wear leveling dinamico: escolha do bloco livre" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    // Gasta os blocos 0 e 1 antes de criar a FTL
    for (int i = 0; i < 5; ++i) { driver.erase_block(0); driver.erase_block(1); }

    PageMappingFTL naive(driver);
    naive.write(10, pattern(10, 1).data());
    PhysicalAddress pba{0, 0};
    naive.lookup(10, pba);
    CHECK(pba.block == 0, "sem wear leveling, o primeiro bloco usado e o de menor numero (bloco 0, ja gasto)");

    flash.format();
    for (int i = 0; i < 5; ++i) { driver.erase_block(0); driver.erase_block(1); }
    WearLevelingFTL wl(driver, false);
    wl.write(10, pattern(10, 1).data());
    wl.lookup(10, pba);
    CHECK(pba.block == 2, "com wear leveling dinamico, o primeiro bloco usado e o de menor P/E (bloco 2)");
    CHECK(std::string(wl.name()).find("dinamico") != std::string::npos, "nome do modo dinamico");
    CHECK(!wl.static_enabled(), "estatico desligado neste modo");
}

static void test_compare_modes() {
    std::cout << "Mesma carga de dados quentes nos tres modos com mapeamento (200000 alteracoes)" << std::endl;
    const int updates = 200000;
    double sd[3];
    uint32_t mx[3];
    uint64_t migrations[3] = {0, 0, 0};
    bool data_ok[3], writes_ok[3], one_valid[3];

    for (int mode = 0; mode < 3; ++mode) {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        PageMappingFTL* ftl = mode == 0 ? new PageMappingFTL(driver) : new WearLevelingFTL(driver, mode == 2);
        std::vector<uint32_t> version = hot_workload(*ftl, updates, writes_ok[mode]);
        data_ok[mode] = all_data_ok(*ftl, version);
        one_valid[mode] = total_valid(driver) == LOGICAL_PAGES;
        sd[mode] = pe_stdev(driver);
        mx[mode] = pe_max(driver);
        if (mode == 2) migrations[mode] = static_cast<WearLevelingFTL*>(ftl)->wl().static_migrations;
        if (mode == 1) migrations[mode] = static_cast<WearLevelingFTL*>(ftl)->wl().static_migrations;
        std::cout << "           " << ftl->name() << ": desvio " << sd[mode] << ", max P/E " << mx[mode] << std::endl;
        delete ftl;
    }

    CHECK(writes_ok[0] && writes_ok[1] && writes_ok[2], "todas as escritas OK nos tres modos");
    CHECK(data_ok[0] && data_ok[1] && data_ok[2], "os 7680 LBAs tem a versao mais recente nos tres modos");
    CHECK(one_valid[1] && one_valid[2], "exatamente uma pagina valida por LBA com wear leveling");
    CHECK(sd[1] < sd[0], "o dinamico reduz o desvio padrao do desgaste");
    CHECK(sd[2] < sd[1], "o estatico reduz ainda mais o desvio padrao");
    CHECK(mx[2] < mx[0], "o bloco mais gasto fica menos gasto com wear leveling");
    CHECK(migrations[1] == 0, "so dinamico: nenhuma migracao de dados frios");
    CHECK(migrations[2] > 0, "dinamico + estatico: dados frios foram migrados");
}

static void test_static_threshold() {
    std::cout << "Wear leveling estatico: limite de diferenca de P/E" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    WearLevelingFTL ftl(driver, true);
    bool ok = true;
    std::vector<uint32_t> version = hot_workload(ftl, 200000, ok);
    uint32_t max_pe = 0, min_data_pe = MAX_PE_CYCLES;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
        const NANDBlock& block = driver.block_info(b);
        max_pe = std::max(max_pe, block.pe_cycles);
        if (block.valid_pages_count > 0) min_data_pe = std::min(min_data_pe, block.pe_cycles);
    }
    std::cout << "           diferenca final entre o bloco mais gasto e o bloco de dados menos gasto: "
              << (max_pe - min_data_pe) << std::endl;
    CHECK(max_pe - min_data_pe <= 2 * WearLevelingFTL::STATIC_WL_THRESHOLD,
          "a diferenca de desgaste fica perto do limite configurado (50)");
    CHECK(driver.stats().page_programs ==
              ftl.stats().host_writes + ftl.gc().pages_copied + ftl.wl().pages_moved,
          "paginas programadas = escritas logicas + copias do GC + copias do estatico");
}

// Correção da Etapa 11: um bloco pouco gasto mas quase sem dados válidos não
// pode ser copiado para o bloco mais gasto (o fim do bloco ficaria livre, o GC
// logo o apagaria de novo e o desgaste dele dispararia).
static void test_static_nearly_empty_block() {
    std::cout << "Wear leveling estatico: bloco frio quase vazio" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    // Bloco 127 já gasto (60 ciclos): é o destino dos dados frios
    for (int i = 0; i < 60; ++i) driver.erase_block(127);
    WearLevelingFTL ftl(driver, true);
    std::vector<uint8_t> buffer(PAGE_SIZE);

    // Escritas 1 a 64: LBAs 0 a 63 enchem o bloco 0
    for (uint32_t lba = 0; lba < 64; ++lba) ftl.write(lba, pattern(lba, 1).data());
    // Escritas 65 a 128: regravam os LBAs 0 a 63 menos o 63. O bloco 0 fica com
    // 1 página válida (LBA 63) e a verificação da escrita 128 age sobre ele.
    for (uint32_t lba = 0; lba < 63; ++lba) ftl.write(lba, pattern(lba, 2).data());
    ftl.write(0, pattern(0, 3).data());

    PhysicalAddress pba{0, 0};
    ftl.lookup(63, pba);
    CHECK(driver.block_info(127).free_pages_count == PAGES_PER_BLOCK && pba.block != 127,
          "o bloco quase vazio NAO vai para o bloco mais gasto (127 continua livre)");
    CHECK(driver.block_info(0).pe_cycles == 1 && driver.block_info(0).valid_pages_count == 0,
          "o bloco pouco gasto foi apagado e volta para os livres");
    CHECK(ftl.read(63, buffer.data()) == FTLResult::OK && buffer == pattern(63, 1),
          "a pagina valida que estava nele foi preservada");

    // Escritas 129 a 192: o bloco 1 (63 paginas validas) passa a ser o mais frio
    // e, por estar quase cheio, vai para o bloco mais gasto (migracao classica).
    for (uint32_t lba = 100; lba < 164; ++lba) ftl.write(lba, pattern(lba, 1).data());
    ftl.lookup(5, pba);
    CHECK(pba.block == 127 && driver.block_info(127).valid_pages_count == 63,
          "um bloco frio quase cheio vai para o bloco mais gasto");
    CHECK(ftl.read(5, buffer.data()) == FTLResult::OK && buffer == pattern(5, 2), "dados migrados corretos");
    CHECK(ftl.wl().static_migrations == 2 && ftl.wl().pages_moved == 64,
          "2 acoes do estatico: 1 pagina recolocada + 63 paginas migradas");
}

static void test_reopen() {
    std::cout << "Reabrir o dispositivo com wear leveling" << std::endl;
    std::vector<uint32_t> version;
    {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        WearLevelingFTL ftl(driver, true);
        bool ok = true;
        version = hot_workload(ftl, 50000, ok);
        flash.save_metadata();
    }
    NANDFlashMemory flash(DATA, META);
    CHECK(flash.open() == OpenResult::OK, "dispositivo reaberto");
    IODriver driver(flash);
    WearLevelingFTL ftl(driver, true);
    CHECK(all_data_ok(ftl, version), "todos os LBAs lidos com a versao certa apos reabrir");
    CHECK(ftl.write(0, pattern(0, 999).data()) == FTLResult::OK, "continua gravando normalmente");
}

int main() {
    std::cout << "=== Teste da FTL com wear leveling ===" << std::endl;
    test_dynamic_choice();
    test_compare_modes();
    test_static_threshold();
    test_static_nearly_empty_block();
    test_reopen();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
