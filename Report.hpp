#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "Experiment.hpp"
#include "ReportPage.hpp"

// ---------------------------------------------------------------------------
// Etapa 10: testes comparativos e tela de resultados.
//
// run_comparison() roda as cargas nos quatro modos da FTL, mostra as tabelas
// no terminal, exporta os CSVs e grava "resultados.html": uma página única,
// sem dependências, que abre em qualquer navegador (inclusive sem internet)
// com os gráficos do efeito do wear leveling:
//   - quantas escritas cada modo aguentou até o primeiro bloco falhar;
//   - quanto da vida total dos blocos foi aproveitada;
//   - o mapa de desgaste dos 128 blocos;
//   - o histograma dos ciclos P/E;
//   - a evolução do desgaste (máximo e média) até a falha;
//   - a tabela com todas as métricas.
// A página é o modelo de ReportPage.hpp com os resultados inseridos em JSON.
// ---------------------------------------------------------------------------

inline const char* const REPORT_FILE = "resultados.html";

struct ScenarioResults {
    WorkloadKind workload = WorkloadKind::HOT;
    std::vector<ExperimentResult> results;
};

namespace report_detail {

// Texto em JSON. "<" vira < para o texto nunca fechar o <script> da página.
inline std::string text(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '<') out += "\\u003c";
        else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else out += static_cast<char>(c);
    }
    return out + "\"";
}

// Número em JSON (sempre com ponto decimal; NaN e infinito viram 0)
inline std::string number(double v) {
    if (!std::isfinite(v)) return "0";
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.precision(10);
    out << v;
    return out.str();
}

template <typename T>
inline std::string list(const std::vector<T>& values) {
    std::string out = "[";
    for (size_t i = 0; i < values.size(); ++i) out += (i ? "," : "") + std::to_string(values[i]);
    return out + "]";
}

inline std::string result(const ExperimentResult& r) {
    const WearSummary& w = r.wear;
    std::ostringstream o;
    o << "{\"chave\":" << text(r.mode) << ",\"nome\":" << text(r.mode_name)
      << ",\"falhou\":" << (r.failed ? "true" : "false") << ",\"escrita_falha\":" << r.failure_write
      << ",\"escritas_carga\":" << r.load_writes << ",\"bloco_falha\":" << r.failed_block
      << ",\"operacoes\":" << r.operations << ",\"wa\":" << number(r.write_amplification)
      << ",\"gc\":" << r.gc_runs << ",\"gc_paginas\":" << r.gc_pages << ",\"migracoes\":" << r.wl_migrations
      << ",\"wl_paginas\":" << r.wl_pages << ",\"media\":" << number(w.mean) << ",\"desvio\":" << number(w.stdev)
      << ",\"cv\":" << number(w.cv) << ",\"min\":" << w.min << ",\"max\":" << w.max
      << ",\"mediana\":" << number(w.median) << ",\"p10\":" << w.p10 << ",\"p90\":" << w.p90
      << ",\"nunca\":" << w.untouched << ",\"bad\":" << w.bad << ",\"vida\":" << number(w.life_used)
      << ",\"segundos\":" << number(r.seconds) << ",\"ruins\":" << list(r.bad_blocks) << ",\"pe\":" << list(r.pe)
      << ",\"evolucao\":[";
    // Cada ponto: [escritas lógicas, média, desvio, máximo]
    for (size_t i = 0; i < r.timeline.size(); ++i) {
        const WearSample& p = r.timeline[i];
        o << (i ? "," : "") << "[" << p.host_writes << "," << number(p.mean) << "," << number(p.stdev) << ","
          << p.max << "]";
    }
    o << "]}";
    return o.str();
}

inline std::string now_text() {
    std::time_t t = std::time(nullptr);
    char buf[32] = "";
    if (const std::tm* local = std::localtime(&t)) std::strftime(buf, sizeof buf, "%d/%m/%Y %H:%M", local);
    return buf;
}

} // namespace report_detail

// Os resultados em JSON, no formato que a página espera
inline std::string report_json(const std::vector<ScenarioResults>& scenarios, const std::string& command,
                               const std::string& generated) {
    using namespace report_detail;
    std::ostringstream o;
    o << "{\"gerado\":" << text(generated) << ",\"comando\":" << text(command) << ",\"geometria\":{\"blocos\":"
      << TOTAL_BLOCKS << ",\"paginas_por_bloco\":" << PAGES_PER_BLOCK << ",\"tamanho_pagina\":" << PAGE_SIZE
      << ",\"ciclos_max\":" << MAX_PE_CYCLES << ",\"paginas_logicas\":" << LOGICAL_PAGES << "},\"cenarios\":[";
    for (size_t s = 0; s < scenarios.size(); ++s) {
        o << (s ? "," : "") << "{\"chave\":" << text(workload_key(scenarios[s].workload))
          << ",\"titulo\":" << text(workload_title(scenarios[s].workload)) << ",\"modos\":[";
        for (size_t i = 0; i < scenarios[s].results.size(); ++i)
            o << (i ? "," : "") << result(scenarios[s].results[i]);
        o << "]}";
    }
    o << "]}";
    return o.str();
}

inline bool write_report(const std::string& file, const std::vector<ScenarioResults>& scenarios,
                         const std::string& command, const std::string& generated = report_detail::now_text()) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (const char* part : REPORT_PAGE_BEFORE) out << part;
    out << report_json(scenarios, command, generated);
    for (const char* part : REPORT_PAGE_AFTER) out << part;
    return static_cast<bool>(out);
}

// Abre o arquivo no navegador padrão (Windows, macOS ou Linux)
inline void open_in_browser(const std::string& file) {
#if defined(_WIN32)
    std::string command = "start \"\" \"" + file + "\"";
#elif defined(__APPLE__)
    std::string command = "open \"" + file + "\"";
#else
    std::string command = "xdg-open \"" + file + "\" >/dev/null 2>&1 &";
#endif
    int ignored = std::system(command.c_str());
    (void)ignored;
}

// Roda as cargas pedidas nos quatro modos, exporta os CSVs e grava a tela
// de resultados. Usado por --comparar, --cenario-* e pelo comando "comparar".
inline int run_comparison(const std::vector<WorkloadKind>& workloads, const std::string& command, bool open,
                          std::ostream& out) {
    std::vector<ScenarioResults> all;
    for (WorkloadKind w : workloads) {
        out << "\n=== Cenario: " << workload_title(w) << " (ate a primeira falha de bloco) ===" << std::endl;
        ScenarioResults s;
        s.workload = w;
        s.results = run_all_modes(w, out);
        out << std::endl;
        print_comparison(s.results, out);
        export_results(s.results, std::string(workload_key(w)) + "_", out);
        all.push_back(std::move(s));
    }
    if (!write_report(REPORT_FILE, all, command)) {
        out << "[ERRO] Nao foi possivel gravar " << REPORT_FILE << std::endl;
        return 1;
    }
    out << "\n[OK] Tela de resultados gravada em " << REPORT_FILE << std::endl;
    if (open) {
        out << "Abrindo no navegador. Se nao abrir, de dois cliques no arquivo " << REPORT_FILE << "." << std::endl;
        open_in_browser(REPORT_FILE);
    }
    return 0;
}
