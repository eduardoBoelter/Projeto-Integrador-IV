// Teste da coleta de métricas e dos experimentos (Etapa 9).
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "../Experiment.hpp"

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

static const char* DATA = "test_metrics_device.bin";
static const char* META = "test_metrics_meta.bin";

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static size_t count_lines(const std::string& file) {
    std::ifstream in(file);
    std::string line;
    size_t n = 0;
    while (std::getline(in, line)) n++;
    return n;
}

static void test_summary() {
    std::cout << "Resumo estatistico do desgaste" << std::endl;
    WearSummary s = summarize({0, 0, 10, 10});
    CHECK(near(s.mean, 5) && near(s.stdev, 5) && near(s.cv, 1), "media 5, desvio 5 e CV 1 para {0, 0, 10, 10}");
    CHECK(s.min == 0 && s.max == 10 && near(s.median, 5) && s.untouched == 2, "minimo, maximo, mediana e nunca apagados");
    CHECK(near(s.life_used, 5.0 / MAX_PE_CYCLES), "vida usada = media / 1000");

    WearSummary uniform = summarize(std::vector<uint32_t>(TOTAL_BLOCKS, 300));
    CHECK(near(uniform.stdev, 0) && near(uniform.cv, 0) && uniform.untouched == 0, "desgaste igual: desvio e CV zero");

    std::vector<uint32_t> ramp(TOTAL_BLOCKS);
    for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) ramp[b] = TOTAL_BLOCKS - 1 - b; // fora de ordem de proposito
    WearSummary r = summarize(ramp);
    CHECK(r.p10 == 12 && r.p90 == 114 && near(r.median, 63.5), "percentis 10 e 90 e mediana de 0..127");
    CHECK(summarize(std::vector<uint32_t>{}).max == 0, "lista vazia nao quebra");
}

static void test_timeline() {
    std::cout << "Evolucao do desgaste (amostragem)" << std::endl;
    NANDFlashMemory flash(DATA, META);
    flash.format();
    IODriver driver(flash);
    WearTimeline timeline;
    for (uint64_t w = 0; w <= 1000000; ++w) timeline.record(w, driver);
    const auto& p = timeline.points();
    bool ascending = true;
    for (size_t i = 1; i < p.size(); ++i) ascending = ascending && p[i].host_writes > p[i - 1].host_writes;
    CHECK(p.size() >= WearTimeline::MAX_SAMPLES / 2 && p.size() <= WearTimeline::MAX_SAMPLES,
          "1 milhao de escritas cabem em 200 a 400 pontos (" + std::to_string(p.size()) + ")");
    CHECK(ascending && p.front().host_writes == 0, "pontos em ordem, comecando na escrita 0");
    CHECK(p.back().host_writes > 900000, "a curva cobre o experimento inteiro");

    timeline.record(1000001, driver, true);
    CHECK(timeline.points().back().host_writes == 1000001, "amostra forcada entra mesmo fora do intervalo");
}

static void test_csv() {
    std::cout << "Exportacao em CSV" << std::endl;
    std::vector<uint32_t> a(TOTAL_BLOCKS, 1), b(TOTAL_BLOCKS, 2);
    CHECK(export_wear_csv("test_metrics_desgaste.csv", {"a", "b"}, {a, b}), "grava o desgaste por bloco");
    std::ifstream in("test_metrics_desgaste.csv");
    std::string header, first;
    std::getline(in, header);
    std::getline(in, first);
    CHECK(header == "bloco,a,b" && first == "0,1,2", "cabecalho e primeira linha corretos");
    CHECK(count_lines("test_metrics_desgaste.csv") == TOTAL_BLOCKS + 1, "uma linha por bloco");

    std::vector<WearSample> points = {{0, 0, 0, 0}, {100, 1.5, 0.5, 3}};
    CHECK(export_timeline_csv("test_metrics_evolucao.csv", points) && count_lines("test_metrics_evolucao.csv") == 3,
          "grava a evolucao (cabecalho + 2 pontos)");
    std::remove("test_metrics_desgaste.csv");
    std::remove("test_metrics_evolucao.csv");
}

static void test_experiments() {
    std::cout << "Experimentos curtos" << std::endl;
    {
        Experiment exp(DATA, META, 40000);
        ExperimentResult direct = exp.run(FTLMode::DIRECT, WorkloadKind::HOT);
        CHECK(direct.failed && direct.wear.max == MAX_PE_CYCLES && direct.wear.bad == 1,
              "modo direto: o bloco 0 falha (escrita " + std::to_string(direct.failure_write) + ")");
        CHECK(direct.load_writes == direct.failure_write - LOGICAL_PAGES, "escritas da carga descontam o preenchimento");
        CHECK(direct.pe.size() == TOTAL_BLOCKS && direct.timeline.size() >= 2, "guarda P/E por bloco e a evolucao");

        ExperimentResult mapping = exp.run(FTLMode::MAPPING, WorkloadKind::HOT);
        CHECK(!mapping.failed && mapping.failure_write == 40000, "mapeamento: sem falha ate o limite de escritas");
        CHECK(mapping.gc_runs > 0 && mapping.write_amplification > 1, "mapeamento: GC executado e WA maior que 1");

        ExperimentResult fs = exp.run(FTLMode::WL_STATIC, WorkloadKind::FILESYSTEM);
        CHECK(!fs.failed && fs.operations > 0 && fs.write_amplification >= 1,
              "carga do sistema de arquivos com WL estatico (" + std::to_string(fs.operations) + " operacoes)");
        CHECK(fs.mode == "estatico" && fs.mode_name.find("estatico") != std::string::npos, "nome do modo registrado");
    }
    std::ifstream leftover(DATA);
    CHECK(!leftover, "o experimento apaga o proprio dispositivo ao terminar");
}

int main() {
    std::cout << "=== Teste da coleta de metricas ===" << std::endl;
    test_summary();
    test_timeline();
    test_csv();
    test_experiments();

    std::remove(DATA);
    std::remove(META);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
