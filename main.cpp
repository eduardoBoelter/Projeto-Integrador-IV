#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "NANDFlash.hpp"
#include "IODriver.hpp"
#include "FTL.hpp"

// Uso:
//   ./simulador                  demonstração da FTL no dispositivo do usuário
//   ./simulador --format         formata o dispositivo antes da demonstração
//   ./simulador --cenario-quente roda o cenário de dados quentes até a 1ª falha
//                                (usa arquivos próprios, não mexe no dispositivo)

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

// Demonstração: grava e regrava o LBA 0 pela FTL de mapeamento direto
static int run_demo(bool force_format) {
    NANDFlashMemory flash("nand_device.bin", "nand_meta.bin");
    if (!open_or_format(flash, force_format)) return 1;

    IODriver driver(flash);
    DirectFTL ftl(driver);
    std::cout << "FTL: " << ftl.name() << " | Capacidade logica: " << ftl.logical_capacity()
              << " LBAs (" << ftl.logical_capacity() * PAGE_SIZE / (1024 * 1024) << " MB)" << std::endl;

    std::vector<uint8_t> buffer(PAGE_SIZE);
    for (int i = 1; i <= 2; ++i) {
        const uint32_t pe_before = driver.block_info(0).pe_cycles;
        auto data = text_page("Versao " + std::to_string(i) + " do LBA 0 (P/E do bloco 0 = " +
                              std::to_string(pe_before) + ")");
        FTLResult written = ftl.write(0, data.data());
        ftl.read(0, buffer.data());
        std::cout << "\nwrite(LBA 0) #" << i << ": " << to_string(written)
                  << " | P/E do bloco 0: " << pe_before << " -> " << driver.block_info(0).pe_cycles
                  << "\nread(LBA 0): \"" << reinterpret_cast<const char*>(buffer.data()) << "\"" << std::endl;
    }
    std::cout << "\nCada alteracao do LBA 0 apagou o bloco 0 inteiro (read-modify-erase-write)." << std::endl;

    const IOStats& s = driver.stats();
    std::cout << "Escritas logicas: " << ftl.stats().host_writes << " | Paginas programadas: " << s.page_programs
              << " | Apagamentos: " << s.block_erases << " | Amplificacao de escrita: " << std::fixed
              << std::setprecision(2) << ftl.write_amplification() << std::endl;

    if (!flash.save_metadata()) {
        std::cerr << "[ERRO] Falha ao salvar os metadados." << std::endl;
        return 1;
    }
    std::cout << "[OK] Metadados salvos em " << flash.meta_file() << "." << std::endl;
    return 0;
}

// Cenário de dados quentes: simula um sistema de arquivos que atualiza o
// tempo todo os mesmos metadados (LBAs 0 a 3: superbloco, tabela de
// alocação e diretório) e, de vez em quando, um arquivo qualquer.
static int run_hot_scenario() {
    std::cout << "=== Cenario de dados quentes: mapeamento direto ===" << std::endl;
    NANDFlashMemory flash("cenario_device.bin", "cenario_meta.bin");
    if (!flash.format()) {
        std::cerr << "[ERRO] Falha ao formatar o dispositivo do cenario." << std::endl;
        return 1;
    }
    IODriver driver(flash);
    DirectFTL ftl(driver);

    std::vector<uint8_t> data(PAGE_SIZE);
    auto fill = [&](uint32_t lba, uint64_t version) {
        for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(lba + version + i);
    };

    // Fase 1: grava o disco inteiro uma vez (como copiar os arquivos para o pendrive)
    for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) {
        fill(lba, 0);
        if (ftl.write(lba, data.data()) != FTLResult::OK) {
            std::cerr << "[ERRO] Falha no preenchimento inicial." << std::endl;
            return 1;
        }
    }
    std::cout << "Fase 1: " << LOGICAL_PAGES << " LBAs gravados (disco cheio)." << std::endl;
    const uint64_t writes_phase1 = ftl.stats().host_writes;
    const uint64_t programs_phase1 = driver.stats().page_programs;

    // Fase 2: 90% das escritas nos metadados (LBAs 0-3), 10% em dados aleatórios
    std::mt19937 rng(2026); // semente fixa: o resultado é sempre o mesmo
    std::uniform_int_distribution<uint32_t> cold_lba(PAGES_PER_BLOCK, LOGICAL_PAGES - 1);
    std::uniform_int_distribution<int> percent(0, 99);
    uint64_t version = 1;
    FTLResult result = FTLResult::OK;
    while (result == FTLResult::OK && version < 10000000) {
        uint32_t lba = (percent(rng) < 90) ? static_cast<uint32_t>(version % 4) : cold_lba(rng);
        fill(lba, version++);
        result = ftl.write(lba, data.data());
    }
    if (result != FTLResult::DEVICE_WORN_OUT) {
        std::cerr << "[ERRO] Cenario terminou com " << to_string(result) << std::endl;
        return 1;
    }

    // Resultados
    const FTLStats& fs = ftl.stats();
    std::vector<uint32_t> pe(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) pe[b] = driver.block_info(b).pe_cycles;
    double mean = 0;
    for (uint32_t v : pe) mean += v;
    mean /= TOTAL_BLOCKS;
    double variance = 0;
    for (uint32_t v : pe) variance += (v - mean) * (v - mean);
    const double stdev = std::sqrt(variance / TOTAL_BLOCKS);
    const size_t untouched = static_cast<size_t>(std::count(pe.begin(), pe.end(), 0u));

    std::cout << "Fase 2: dispositivo falhou." << std::endl
              << "\nPrimeira falha: escrita logica n. " << fs.first_failure_write << " (bloco "
              << fs.first_failed_block << ")" << std::endl
              << "Escritas logicas totais: " << fs.host_writes << " (" << writes_phase1
              << " na fase 1 e " << fs.host_writes - writes_phase1 << " na fase 2)" << std::endl
              << std::fixed << std::setprecision(2)
              << "Apagamentos por bloco: media " << mean << " | desvio padrao " << stdev << " | maximo "
              << *std::max_element(pe.begin(), pe.end()) << std::endl
              << "Blocos nunca apagados: " << untouched << " de " << TOTAL_BLOCKS << std::endl
              << "Amplificacao de escrita: " << ftl.write_amplification() << " no total, "
              << static_cast<double>(driver.stats().page_programs - programs_phase1) /
                     static_cast<double>(fs.host_writes - writes_phase1)
              << " na fase 2" << std::endl;

    std::vector<uint32_t> order(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) order[b] = b;
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return pe[a] > pe[b]; });
    std::cout << "\nBlocos mais desgastados:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "  bloco " << std::setw(3) << order[i] << ": " << std::setw(4) << pe[order[i]]
                  << " ciclos P/E" << (driver.block_info(order[i]).is_bad_block ? " (bad block)" : "")
                  << std::endl;
    }

    // CSV para gerar o gráfico de desgaste por bloco
    std::ofstream csv("desgaste_direto.csv");
    csv << "bloco,ciclos_pe\n";
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) csv << b << "," << pe[b] << "\n";
    std::cout << "\n[OK] Desgaste por bloco exportado em desgaste_direto.csv" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    const std::string option = argc > 1 ? argv[1] : "";
    if (option == "--cenario-quente") return run_hot_scenario();

    std::cout << "=== Simulador de Memoria NAND Flash ===" << std::endl;
    return run_demo(option == "--format");
}
