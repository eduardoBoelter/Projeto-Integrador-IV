#pragma once

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Metrics.hpp"
#include "Workload.hpp"

// ---------------------------------------------------------------------------
// Etapa 9: experimentos com coleta de métricas.
//
// Um experimento formata um dispositivo próprio (não mexe no do shell), aplica
// uma carga num modo da FTL até o primeiro bloco virar bad block e devolve
// todas as métricas, inclusive a evolução do desgaste ao longo das escritas.
//
// Cargas disponíveis:
//   quente: enche o disco e depois manda 90% das escritas para os LBAs 0 a 3
//           (metadados) e 10% para LBAs aleatórios. É a carga da 2ª entrega.
//   fs:     uso de pendrive pelo sistema de arquivos da Etapa 7 (fotos e
//           músicas que não mudam + documentos editados o tempo todo).
// ---------------------------------------------------------------------------

enum class WorkloadKind { HOT, FILESYSTEM };

inline const char* workload_key(WorkloadKind w) { return w == WorkloadKind::HOT ? "quente" : "fs"; }
inline const char* workload_title(WorkloadKind w) {
    return w == WorkloadKind::HOT ? "Dados quentes (LBAs 0 a 3)" : "Uso de pendrive pelo sistema de arquivos";
}

struct ExperimentResult {
    std::string mode;             // chave: direto, mapeamento, dinamico, estatico
    std::string mode_name;        // nome legível da FTL
    WorkloadKind workload = WorkloadKind::HOT;
    bool failed = false;          // chegou à primeira falha de bloco?
    uint64_t failure_write = 0;   // escrita lógica (total) em que o 1º bloco falhou
    uint32_t failed_block = 0;
    uint64_t load_writes = 0;     // escritas lógicas da fase de carga (depois do preenchimento)
    uint64_t operations = 0;      // operações do sistema de arquivos (carga fs)
    double write_amplification = 0; // só da fase de carga
    uint64_t gc_runs = 0, gc_pages = 0;
    uint64_t wl_migrations = 0, wl_pages = 0;
    WearSummary wear;
    std::vector<uint32_t> pe;
    std::vector<uint32_t> bad_blocks; // blocos que viraram bad block
    std::vector<WearSample> timeline;
    double seconds = 0;
};

class Experiment {
private:
    std::string data_file, meta_file;
    uint64_t max_writes;

    static void fill(std::vector<uint8_t>& data, uint32_t lba, uint64_t version) {
        for (size_t i = 0; i < PAGE_SIZE; ++i) data[i] = static_cast<uint8_t>(lba + version + i);
    }

public:
    explicit Experiment(const std::string& data = "experimento_device.bin",
                        const std::string& meta = "experimento_meta.bin",
                        uint64_t write_limit = 50000000)
        : data_file(data), meta_file(meta), max_writes(write_limit) {}

    ~Experiment() {
        std::remove(data_file.c_str());
        std::remove(meta_file.c_str());
    }

    ExperimentResult run(FTLMode mode, WorkloadKind workload) {
        auto start = std::chrono::steady_clock::now();
        ExperimentResult r;
        r.mode = mode_key(mode);
        r.workload = workload;

        NANDFlashMemory flash(data_file, meta_file);
        if (!flash.format()) return r;
        IODriver driver(flash);
        std::unique_ptr<FTL> ftl = make_ftl(mode, driver);
        r.mode_name = ftl->name();
        WearTimeline timeline;
        uint64_t writes_before = 0, programs_before = 0;
        auto failed = [&]() { return ftl->stats().first_failure_write != 0; };

        if (workload == WorkloadKind::HOT) {
            std::vector<uint8_t> data(PAGE_SIZE);
            for (uint32_t lba = 0; lba < LOGICAL_PAGES; ++lba) {
                fill(data, lba, 0);
                ftl->write(lba, data.data());
            }
            writes_before = ftl->stats().host_writes;
            programs_before = driver.stats().page_programs;
            timeline.record(writes_before, driver, true);

            std::mt19937 rng(2026); // semente fixa: o resultado é sempre o mesmo
            std::uniform_int_distribution<uint32_t> cold_lba(PAGES_PER_BLOCK, LOGICAL_PAGES - 1);
            std::uniform_int_distribution<int> percent(0, 99);
            uint64_t version = 1;
            FTLResult result = FTLResult::OK;
            while (result == FTLResult::OK && !failed() && ftl->stats().host_writes < max_writes) {
                uint32_t lba = (percent(rng) < 90) ? static_cast<uint32_t>(version % 4) : cold_lba(rng);
                fill(data, lba, version++);
                result = ftl->write(lba, data.data());
                timeline.record(ftl->stats().host_writes, driver);
            }
        } else {
            SimpleFS fs(*ftl);
            fs.format();
            FSWorkload load(fs);
            load.prepare();
            writes_before = ftl->stats().host_writes;
            programs_before = driver.stats().page_programs;
            timeline.record(writes_before, driver, true);
            FSResult result = FSResult::OK;
            while (result == FSResult::OK && !failed() && ftl->stats().host_writes < max_writes) {
                result = load.step();
                timeline.record(ftl->stats().host_writes, driver);
            }
            r.operations = load.stats().steps;
        }
        timeline.record(ftl->stats().host_writes, driver, true);

        const FTLStats& fs_stats = ftl->stats();
        r.failed = failed();
        r.failure_write = r.failed ? fs_stats.first_failure_write : fs_stats.host_writes;
        r.failed_block = fs_stats.first_failed_block;
        r.load_writes = r.failure_write - writes_before;
        uint64_t load_programs = driver.stats().page_programs - programs_before;
        uint64_t load_host = fs_stats.host_writes - writes_before;
        r.write_amplification = load_host ? static_cast<double>(load_programs) / static_cast<double>(load_host) : 0;
        if (auto* mapping = dynamic_cast<PageMappingFTL*>(ftl.get())) {
            r.gc_runs = mapping->gc().runs;
            r.gc_pages = mapping->gc().pages_copied;
        }
        if (auto* wl = dynamic_cast<WearLevelingFTL*>(ftl.get())) {
            r.wl_migrations = wl->wl().static_migrations;
            r.wl_pages = wl->wl().pages_moved;
        }
        r.pe = pe_counts(driver);
        for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b)
            if (driver.block_info(b).is_bad_block) r.bad_blocks.push_back(b);
        r.wear = summarize(driver);
        r.timeline = timeline.points();
        r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return r;
    }
};

// Roda os quatro modos numa carga e mostra o andamento
inline std::vector<ExperimentResult> run_all_modes(WorkloadKind workload, std::ostream& out) {
    std::vector<ExperimentResult> results;
    Experiment experiment;
    for (FTLMode mode : {FTLMode::DIRECT, FTLMode::MAPPING, FTLMode::WL_DYNAMIC, FTLMode::WL_STATIC}) {
        out << "  " << std::left << std::setw(11) << mode_key(mode) << std::right << "... " << std::flush;
        ExperimentResult r = experiment.run(mode, workload);
        out << (r.failed ? "1a falha na escrita logica " : "sem falha ate ") << r.failure_write << " (bloco "
            << r.failed_block << "), " << std::fixed << std::setprecision(1) << r.seconds << " s" << std::defaultfloat
            << std::endl;
        results.push_back(r);
    }
    return results;
}

// Tabela comparativa em texto (terminal)
inline void print_comparison(const std::vector<ExperimentResult>& results, std::ostream& out) {
    out << std::left << std::setw(11) << "Modo" << std::right << std::setw(13) << "Escritas*" << std::setw(10)
        << "Media P/E" << std::setw(9) << "Desvio" << std::setw(7) << "CV" << std::setw(7) << "Min" << std::setw(6)
        << "Max" << std::setw(8) << "Vida %" << std::setw(7) << "WA" << std::setw(9) << "GC" << std::setw(8)
        << "Migr." << std::endl;
    out << std::fixed;
    for (const auto& r : results) {
        out << std::left << std::setw(11) << r.mode << std::right << std::setw(13) << r.load_writes
            << std::setprecision(1) << std::setw(10) << r.wear.mean << std::setw(9) << r.wear.stdev
            << std::setprecision(2) << std::setw(7) << r.wear.cv << std::setw(7) << r.wear.min << std::setw(6)
            << r.wear.max << std::setprecision(1) << std::setw(8) << 100 * r.wear.life_used << std::setprecision(2)
            << std::setw(7) << r.write_amplification << std::setw(9) << r.gc_runs << std::setw(8)
            << r.wl_migrations << std::endl;
    }
    out << std::defaultfloat << "* escritas logicas da fase de carga ate a primeira falha de bloco" << std::endl;
}

// Exporta os CSVs de um conjunto de resultados com o prefixo dado
inline void export_results(const std::vector<ExperimentResult>& results, const std::string& prefix,
                           std::ostream& out) {
    std::vector<std::string> names;
    std::vector<std::vector<uint32_t>> pe;
    for (const auto& r : results) {
        names.push_back(r.mode);
        pe.push_back(r.pe);
        export_timeline_csv(prefix + "evolucao_" + r.mode + ".csv", r.timeline);
    }
    export_wear_csv(prefix + "desgaste_por_bloco.csv", names, pe);

    std::ofstream summary(prefix + "resumo.csv");
    summary << "modo,escritas_ate_falha,escritas_fase_carga,bloco_que_falhou,media_pe,desvio_pe,cv,min_pe,max_pe,"
               "mediana_pe,blocos_nunca_apagados,vida_usada,amplificacao,gc_execucoes,gc_paginas,"
               "wl_migracoes,wl_paginas,operacoes_fs\n";
    for (const auto& r : results) {
        summary << r.mode << "," << r.failure_write << "," << r.load_writes << "," << r.failed_block << ","
                << r.wear.mean << "," << r.wear.stdev << "," << r.wear.cv << "," << r.wear.min << "," << r.wear.max
                << "," << r.wear.median << "," << r.wear.untouched << "," << r.wear.life_used << ","
                << r.write_amplification << "," << r.gc_runs << "," << r.gc_pages << "," << r.wl_migrations << ","
                << r.wl_pages << "," << r.operations << "\n";
    }
    out << "[OK] CSVs exportados: " << prefix << "resumo.csv, " << prefix << "desgaste_por_bloco.csv e "
        << prefix << "evolucao_<modo>.csv" << std::endl;
}
