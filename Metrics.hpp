#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include "IODriver.hpp"

// ---------------------------------------------------------------------------
// Etapa 9: coleta de métricas de desgaste.
//
// Tudo é calculado a partir dos contadores P/E da camada física, lidos pelo
// driver. As métricas respondem à pergunta de pesquisa:
//   - média, desvio padrão e coeficiente de variação dos ciclos P/E: quanto o
//     desgaste ficou uniforme entre os blocos;
//   - momento da primeira falha: quantas escritas lógicas o dispositivo
//     aguentou até o primeiro bloco virar bad block;
//   - vida aproveitada: quanto da vida total dos blocos foi usada até a falha;
//   - amplificação de escrita: quanto trabalho extra a FTL fez.
// ---------------------------------------------------------------------------

struct WearSummary {
    double mean = 0;        // média de ciclos P/E por bloco
    double stdev = 0;       // desvio padrão populacional
    double cv = 0;          // coeficiente de variação = desvio / média
    uint32_t min = 0;
    uint32_t max = 0;
    double median = 0;
    uint32_t p10 = 0;       // 10% dos blocos têm até este desgaste
    uint32_t p90 = 0;       // 90% dos blocos têm até este desgaste
    uint32_t untouched = 0; // blocos nunca apagados
    uint32_t bad = 0;       // bad blocks
    double life_used = 0;   // média / MAX_PE_CYCLES: fração da vida total já consumida
};

inline std::vector<uint32_t> pe_counts(const IODriver& driver) {
    std::vector<uint32_t> pe(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) pe[b] = driver.block_info(b).pe_cycles;
    return pe;
}

inline WearSummary summarize(const std::vector<uint32_t>& pe, uint32_t bad_blocks = 0) {
    WearSummary s;
    if (pe.empty()) return s;
    const double n = static_cast<double>(pe.size());
    for (uint32_t v : pe) s.mean += v;
    s.mean /= n;
    double var = 0;
    for (uint32_t v : pe) var += (v - s.mean) * (v - s.mean);
    s.stdev = std::sqrt(var / n);
    s.cv = s.mean > 0 ? s.stdev / s.mean : 0;

    std::vector<uint32_t> sorted = pe;
    std::sort(sorted.begin(), sorted.end());
    s.min = sorted.front();
    s.max = sorted.back();
    size_t mid = sorted.size() / 2;
    s.median = sorted.size() % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2.0;
    s.p10 = sorted[static_cast<size_t>(0.1 * (sorted.size() - 1))];
    s.p90 = sorted[static_cast<size_t>(0.9 * (sorted.size() - 1))];
    s.untouched = static_cast<uint32_t>(std::count(pe.begin(), pe.end(), 0u));
    s.bad = bad_blocks;
    s.life_used = s.mean / MAX_PE_CYCLES;
    return s;
}

inline WearSummary summarize(const IODriver& driver) {
    uint32_t bad = 0;
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b)
        if (driver.block_info(b).is_bad_block) bad++;
    return summarize(pe_counts(driver), bad);
}

// Um ponto da evolução do desgaste ao longo das escritas
struct WearSample {
    uint64_t host_writes = 0;
    double mean = 0;
    double stdev = 0;
    uint32_t max = 0;
};

// Guarda amostras do desgaste durante um experimento. Começa com uma amostra
// a cada `interval` escritas lógicas; quando passa de MAX_SAMPLES, descarta
// uma amostra sim, outra não, e dobra o intervalo. Assim a curva sempre tem
// entre 200 e 400 pontos, seja o experimento curto (modo direto) ou longo.
class WearTimeline {
public:
    static constexpr size_t MAX_SAMPLES = 400;

private:
    uint64_t interval;
    uint64_t next_at;
    std::vector<WearSample> samples;

public:
    explicit WearTimeline(uint64_t first_interval = 50) : interval(first_interval), next_at(0) {}

    void record(uint64_t host_writes, const IODriver& driver, bool force = false) {
        if (!force && host_writes < next_at) return;
        WearSummary s = summarize(driver);
        samples.push_back({host_writes, s.mean, s.stdev, s.max});
        next_at = host_writes + interval;
        if (samples.size() > MAX_SAMPLES) {
            std::vector<WearSample> kept;
            for (size_t i = 0; i < samples.size(); i += 2) kept.push_back(samples[i]);
            if (kept.back().host_writes != samples.back().host_writes) kept.push_back(samples.back());
            samples.swap(kept);
            interval *= 2;
            next_at = samples.back().host_writes + interval;
        }
    }

    const std::vector<WearSample>& points() const { return samples; }
};

// --------------------------- exportação em CSV ---------------------------

// Uma coluna de ciclos P/E por modo: bloco,<modo1>,<modo2>,...
inline bool export_wear_csv(const std::string& file, const std::vector<std::string>& names,
                            const std::vector<std::vector<uint32_t>>& pe) {
    std::ofstream out(file);
    if (!out) return false;
    out << "bloco";
    for (const auto& n : names) out << "," << n;
    out << "\n";
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
        out << b;
        for (const auto& col : pe) out << "," << (b < col.size() ? col[b] : 0);
        out << "\n";
    }
    return static_cast<bool>(out);
}

inline bool export_timeline_csv(const std::string& file, const std::vector<WearSample>& points) {
    std::ofstream out(file);
    if (!out) return false;
    out << "escritas_logicas,media_pe,desvio_pe,max_pe\n";
    for (const auto& p : points) out << p.host_writes << "," << p.mean << "," << p.stdev << "," << p.max << "\n";
    return static_cast<bool>(out);
}
