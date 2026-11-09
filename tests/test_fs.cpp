// Teste do sistema de arquivos mínimo e do gerador de carga (Etapa 7).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "../Workload.hpp"

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

static const char* DATA = "test_fs_device.bin";
static const char* META = "test_fs_meta.bin";

static std::vector<uint8_t> bytes(size_t size, uint8_t seed) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(seed + i * 3);
    return data;
}

static void test_basic_operations() {
    std::cout << "Operacoes basicas" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);
    SimpleFS fs(ftl);

    CHECK(fs.mount() == FSResult::NOT_FORMATTED, "dispositivo novo nao tem sistema de arquivos");
    CHECK(fs.format() == FSResult::OK, "formatacao");
    CHECK(fs.free_pages() == FS_DATA_LBAS, "7670 LBAs de dados livres");
    CHECK(fs.create("a.txt") == FSResult::OK && fs.exists("a.txt"), "cria arquivo vazio");
    CHECK(fs.create("a.txt") == FSResult::ALREADY_EXISTS, "nome repetido e recusado");
    CHECK(fs.create("") == FSResult::NAME_INVALID && fs.create("nome com espaco") == FSResult::NAME_INVALID,
          "nome vazio ou com espaco e recusado");

    auto big = bytes(5 * PAGE_SIZE + 100, 7); // 6 paginas
    CHECK(fs.write_file("a.txt", big) == FSResult::OK, "grava 10340 bytes");
    CHECK(fs.free_pages() == FS_DATA_LBAS - 6, "ocupou 6 LBAs de dados");
    std::vector<uint8_t> back;
    CHECK(fs.read_file("a.txt", back) == FSResult::OK && back == big, "le o mesmo conteudo de volta");

    auto small = bytes(100, 9);
    fs.write_file("a.txt", small);
    CHECK(fs.free_pages() == FS_DATA_LBAS - 1, "regravar menor libera os LBAs que sobraram");
    fs.read_file("a.txt", back);
    CHECK(back == small, "conteudo novo correto");

    fs.write_file("b.txt", bytes(3000, 1));
    auto files = fs.list();
    CHECK(files.size() == 2 && files[0].name == "a.txt" && files[1].name == "b.txt" && files[1].size == 3000,
          "lista os dois arquivos com tamanho");
    CHECK(fs.remove("a.txt") == FSResult::OK && !fs.exists("a.txt") && fs.free_pages() == FS_DATA_LBAS - 2,
          "apagar libera o espaco");
    CHECK(fs.remove("a.txt") == FSResult::NOT_FOUND, "apagar arquivo inexistente");
    CHECK(fs.write_file("enorme.bin", std::vector<uint8_t>((FS_DATA_LBAS + 1) * PAGE_SIZE, 0)) == FSResult::NO_SPACE,
          "arquivo maior que o disco e recusado");
}

static void test_directory_full() {
    std::cout << "Diretorio cheio" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    PageMappingFTL ftl(driver);
    SimpleFS fs(ftl);
    fs.format();
    bool ok = true;
    for (uint32_t i = 0; i < FS_MAX_FILES; ++i) ok = ok && fs.create("f" + std::to_string(i)) == FSResult::OK;
    CHECK(ok, "cria 32 arquivos");
    CHECK(fs.create("extra") == FSResult::DIRECTORY_FULL, "o 33o arquivo e recusado");
}

static void test_hot_metadata() {
    std::cout << "Metadados quentes" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    DirectFTL ftl(driver);
    SimpleFS fs(ftl);
    fs.format();
    for (int i = 0; i < 20; ++i) fs.write_file("doc.txt", bytes(4000, static_cast<uint8_t>(i)));
    const FSStats& s = fs.stats();
    CHECK(s.operations == 22, "22 operacoes (formatar, criar e 20 gravacoes)");
    CHECK(s.metadata_writes >= 2 * s.operations, "cada operacao regrava pelo menos superbloco e diretorio");
    CHECK(s.metadata_writes > s.data_writes, "ha mais escritas de metadados que de conteudo");
    // No mapeamento direto, os LBAs 0 a 9 (metadados) ficam todos no bloco 0
    CHECK(driver.block_info(0).pe_cycles > 20 && driver.block_info(1).pe_cycles <= 1,
          "no modo direto, o bloco 0 (metadados) e apagado a cada operacao");
}

static void test_persistence() {
    std::cout << "Persistencia (remontar depois de reabrir)" << std::endl;
    auto content = bytes(9000, 42);
    {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        WearLevelingFTL ftl(driver, true);
        SimpleFS fs(ftl);
        fs.format();
        fs.write_file("relatorio.txt", content);
        fs.write_file("notas.txt", bytes(10, 1));
        fs.remove("notas.txt");
        flash.save_metadata();
    }
    NANDFlashMemory flash(DATA, META);
    flash.open();
    IODriver driver(flash);
    WearLevelingFTL ftl(driver, true);
    SimpleFS fs(ftl);
    CHECK(fs.mount() == FSResult::OK, "monta o sistema de arquivos gravado");
    std::vector<uint8_t> back;
    CHECK(fs.read_file("relatorio.txt", back) == FSResult::OK && back == content, "arquivo intacto");
    CHECK(fs.list().size() == 1 && !fs.exists("notas.txt"), "arquivo apagado continua apagado");
    CHECK(fs.free_pages() == FS_DATA_LBAS - 5, "espaco livre recalculado pela FAT");
}

static void test_workload() {
    std::cout << "Gerador de carga nos quatro modos da FTL" << std::endl;
    for (int mode = 0; mode < 4; ++mode) {
        NANDFlashMemory flash(DATA, META);
        flash.format();
        IODriver driver(flash);
        std::unique_ptr<FTL> ftl;
        if (mode == 0) ftl = std::make_unique<DirectFTL>(driver);
        else if (mode == 1) ftl = std::make_unique<PageMappingFTL>(driver);
        else ftl = std::make_unique<WearLevelingFTL>(driver, mode == 3);
        SimpleFS fs(*ftl);
        fs.format();
        FSWorkload load(fs);
        bool ok = load.prepare() == FSResult::OK;
        // No modo direto o bloco 0 morre em poucas centenas de operações
        const int steps = mode == 0 ? 150 : 3000;
        for (int i = 0; i < steps && ok; ++i) ok = load.step() == FSResult::OK;
        const WorkloadStats& w = load.stats();
        bool cold = w.cold_bytes >= 0.55 * FS_DATA_LBAS * PAGE_SIZE;
        CHECK(ok && cold && w.edits > w.creates && w.creates > w.removes,
              std::string(ftl->name()) + ": carga executada (" + std::to_string(w.cold_files) + " arquivos frios, " +
                  std::to_string(w.edits) + " edicoes)");
    }
}

int main() {
    std::cout << "=== Teste do sistema de arquivos minimo ===" << std::endl;
    test_basic_operations();
    test_directory_full();
    test_hot_metadata();
    test_persistence();
    test_workload();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
