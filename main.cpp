#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "NANDFlash.hpp"
#include "IODriver.hpp"
#include "FTL.hpp"

// Uso:
//   ./simulador                    demonstração da FTL com mapeamento no dispositivo do usuário
//   ./simulador --format           formata o dispositivo antes da demonstração
//   ./simulador --cenario-quente   roda o cenário de dados quentes até a 1ª falha nos dois
//                                  modos já implementados e compara os resultados
//                                  (usa arquivos próprios, não mexe no dispositivo)

// Abre o dispositivo existente ou formata um novo
static bool open_or_format(NANDFlashMemory& flash, bool force_format) {
    OpenResult opened = force_format ? OpenResult::NO_DEVICE : flash.open();
    if (opened == OpenResult::OK) {
        std::cout << "[OK] Dispositivo existente carregado (desgaste preservado)." << std::endl;
        return true;
    }
    if (opened == OpenResult::INVALID_FORMAT || opened == OpenResult::GEOMETRY_MISMATCH) {
        std::cout << "[AVISO] Metadados invalidos ou de outra geometria; formatando." << std::endl;
    }
    if (!flash.format()) {
        std::cerr << "[ERRO] Falha ao formatar o dispositivo." << std::endl;
        return false;
    }
    std::cout << "[OK] Novo dispositivo formatado (" << flash.data_file() << " e "
              << flash.meta_file() << ")." << std::endl;
    return true;
}

// Página de 2 KB com um texto no início e o resto em 0xFF
static std::vector<uint8_t> text_page(const std::string& text) {
    std::vector<uint8_t> page(PAGE_SIZE, ERASED_BYTE);
    std::memcpy(page.data(), text.c_str(), std::min(text.size() + 1, PAGE_SIZE));
    return page;
}

static std::string pba_text(const PageMappingFTL& ftl, uint32_t lba) {
    PhysicalAddress pba{0, 0};
    if (!ftl.lookup(lba, pba)) return "(nao mapeado)";
    return "(bloco " + std::to_string(pba.block) + ", pagina " + std::to_string(pba.page) + ")";
}

// Demonstração: grava e altera o LBA 0 pela FTL com mapeamento de páginas.
// Cada alteração vai para uma página nova; nenhuma apaga bloco.
static int run_demo(bool force_format) {
    NANDFlashMemory flash("nand_device.bin", "nand_meta.bin");
    if (!open_or_format(flash, force_format)) return 1;

    IODriver driver(flash);
    PageMappingFTL ftl(driver);
    std::cout << "FTL: " << ftl.name() << " | Capacidade logica: " << ftl.logical_capacity()
              << " LBAs (" << ftl.logical_capacity() * PAGE_SIZE / (1024 * 1024) << " MB)"
              << " | Blocos livres: " << ftl.free_blocks() << std::endl;
    std::cout << "LBA 0 esta em " << pba_text(ftl, 0) << std::endl;

    std::vector<uint8_t> buffer(PAGE_SIZE);
    for (int i = 1; i <= 3; ++i) {
        auto data = text_page("Versao " + std::to_string(i) + " do LBA 0");
        FTLResult written = ftl.write(0, data.data());
        ftl.read(0, buffer.data());
        std::cout << "\nwrite(LBA 0) #" << i << ": " << to_string(written) << " -> agora em "
                  << pba_text(ftl, 0) << "\nread(LBA 0): \"" << reinterpret_cast<const char*>(buffer.data())
                  << "\"" << std::endl;
    }
    std::cout << "\nCada alteracao foi para uma pagina nova; a versao anterior virou INVALIDA." << std::endl;

    const IOStats& s = driver.stats();
    std::cout << "Escritas logicas: " << ftl.stats().host_writes << " | Paginas programadas: " << s.page_programs
              << " | Paginas invalidadas: " << s.page_invalidations << " | Apagamentos: " << s.block_erases
              << std::endl;

    if (!flash.save_metadata()) {
        std::cerr << "[ERRO] Falha ao salvar os metadados." << std::endl;
        return 1;
    }
    std::cout << "[OK] Metadados salvos em " << flash.meta_file() << "." << std::endl;
    return 0;
}

// Resultado de um cenário, para a tabela comparativa
struct ScenarioResult {
    std::string mode;
    uint64_t phase2_writes = 0;
    uint64_t first_failure = 0;
    double mean = 0, stdev = 0;
    uint32_t max_pe = 0;
    size_t untouched = 0;
    double wa_phase2 = 0;
    uint64_t gc_runs = 0;
};

// Cenário de dados quentes: simula um sistema de arquivos que atualiza o
// tempo todo os mesmos metadados (LBAs 0 a 3: superbloco, tabela de
// alocação e diretório) e, de vez em quando, um arquivo qualquer.
// Roda até a primeira falha de bloco.
static bool run_hot_scenario(const std::string& mode, ScenarioResult& out) {
    NANDFlashMemory flash("cenario_device.bin", "cenario_meta.bin");
    if (!flash.format()) {
        std::cerr << "[ERRO] Falha ao formatar o dispositivo do cenario." << std::endl;
        return false;
    }
    IODriver driver(flash);
    std::unique_ptr<FTL> ftl;
    PageMappingFTL* mapping = nullptr;
    if (mode == "direto") {
        ftl = std::make_unique<DirectFTL>(driver);
    } else {
        mapping = new PageMappingFTL(driver);
        ftl.reset(mapping);
    }
    std::cout << "\n=== Cenario de dados quentes: " << ftl->name() << " ===" << std::endl;

    std::vector<uint8_t> data(PAGE_SIZE);
    auto fill = [&](uint32_t lba, uint64_t version) {
        for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(lba + version + i);
    };

    // Fase 1: grava o disco inteiro uma vez (como copiar os arquivos para o pendrive)
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) {
        fill(lba, 0);
        if (ftl->write(lba, data.data()) != FTLResult::OK) {
            std::cerr << "[ERRO] Falha no preenchimento inicial." << std::endl;
            return false;
        }
    }
    const uint64_t writes_phase1 = ftl->stats().host_writes;
    const uint64_t programs_phase1 = driver.stats().page_programs;
    std::cout << "Fase 1: " << LOGICAL_PAGES << " LBAs gravados (disco cheio)." << std::endl;

    // Fase 2: 90% das escritas nos metadados (LBAs 0-3), 10% em dados aleatórios,
    // até o primeiro bloco virar bad block
    std::mt19937 rng(2026); // semente fixa: o resultado é sempre o mesmo
    std::uniform_int_distribution<uint32_t> cold_lba(PAGES_PER_BLOCK, LOGICAL_PAGES - 1);
    std::uniform_int_distribution<int> percent(0, 99);
    uint64_t version = 1;
    FTLResult result = FTLResult::OK;
    while (result == FTLResult::OK && ftl->stats().first_failure_write == 0 && version < 50000000) {
        uint32_t lba = (percent(rng) < 90) ? static_cast<uint32_t>(version % 4) : cold_lba(rng);
        fill(lba, version++);
        result = ftl->write(lba, data.data());
    }
    if (ftl->stats().first_failure_write == 0) {
        std::cerr << "[ERRO] Cenario terminou sem falha de bloco (" << to_string(result) << ")" << std::endl;
        return false;
    }

    // Métricas
    const FTLStats& fs = ftl->stats();
    std::vector<uint32_t> pe(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) pe[b] = driver.block_info(b).pe_cycles;
    double mean = 0;
    for (uint32_t v : pe) mean += v;
    mean /= TOTAL_BLOCKS;
    double variance = 0;
    for (uint32_t v : pe) variance += (v - mean) * (v - mean);

    out.mode = mode;
    out.phase2_writes = fs.first_failure_write - writes_phase1;
    out.first_failure = fs.first_failure_write;
    out.mean = mean;
    out.stdev = std::sqrt(variance / TOTAL_BLOCKS);
    out.max_pe = *std::max_element(pe.begin(), pe.end());
    out.untouched = static_cast<size_t>(std::count(pe.begin(), pe.end(), 0u));
    out.wa_phase2 = static_cast<double>(driver.stats().page_programs - programs_phase1) /
                    static_cast<double>(fs.host_writes - writes_phase1);
    out.gc_runs = mapping ? mapping->gc().runs : 0;

    std::cout << "Fase 2: primeira falha na escrita logica n. " << fs.first_failure_write << " (bloco "
              << fs.first_failed_block << "), " << out.phase2_writes << " escritas apos o preenchimento."
              << std::endl
              << std::fixed << std::setprecision(2)
              << "Apagamentos por bloco: media " << out.mean << " | desvio padrao " << out.stdev
              << " | maximo " << out.max_pe << " | blocos nunca apagados: " << out.untouched << std::endl
              << "Amplificacao de escrita na fase 2: " << out.wa_phase2;
    if (mapping) {
        std::cout << " | GC: " << mapping->gc().runs << " execucoes, " << mapping->gc().pages_copied
                  << " paginas copiadas";
    }
    std::cout << std::endl;

    std::vector<uint32_t> order(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) order[b] = b;
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return pe[a] > pe[b]; });
    std::cout << "Blocos mais desgastados:";
    for (int i = 0; i < 5; ++i) std::cout << " " << order[i] << " (" << pe[order[i]] << ")";
    std::cout << std::endl;

    // CSV para gerar o gráfico de desgaste por bloco
    const std::string csv_name = "desgaste_" + mode + ".csv";
    std::ofstream csv(csv_name);
    csv << "bloco,ciclos_pe\n";
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) csv << b << "," << pe[b] << "\n";
    std::cout << "[OK] Desgaste por bloco exportado em " << csv_name << std::endl;
    return true;
}

static int run_comparison() {
    std::vector<ScenarioResult> results;
    for (const std::string mode : {"direto", "mapeamento"}) {
        ScenarioResult r;
        if (!run_hot_scenario(mode, r)) return 1;
        results.push_back(r);
    }

    std::cout << "\n=== Comparacao (ate a primeira falha de bloco) ===" << std::endl
              << std::left << std::setw(12) << "Modo" << std::right << std::setw(14) << "Escritas f2"
              << std::setw(12) << "Media P/E" << std::setw(12) << "Desvio" << std::setw(8) << "Max"
              << std::setw(10) << "CV" << std::setw(12) << "Intocados" << std::setw(10) << "WA f2" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    // CV = coeficiente de variação (desvio / média): compara a uniformidade do desgaste
    // entre modos com médias muito diferentes
    for (const auto& r : results) {
        std::cout << std::left << std::setw(12) << r.mode << std::right << std::setw(14) << r.phase2_writes
                  << std::setw(12) << r.mean << std::setw(12) << r.stdev << std::setw(8) << r.max_pe
                  << std::setw(10) << (r.mean > 0 ? r.stdev / r.mean : 0.0) << std::setw(12) << r.untouched << std::setw(10) << r.wa_phase2 << std::endl;
    }
    std::remove("cenario_device.bin");
    std::remove("cenario_meta.bin");
    return 0;
}

int main(int argc, char* argv[]) {
    const std::string option = argc > 1 ? argv[1] : "";
    if (option == "--cenario-quente") return run_comparison();

    std::cout << "=== Simulador de Memoria NAND Flash ===" << std::endl;
    return run_demo(option == "--format");
}
