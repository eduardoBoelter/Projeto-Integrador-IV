// Teste da tela de resultados (Etapa 10). Não roda simulação: monta resultados
// de exemplo, grava a página e confere o conteúdo.
// Cada CHECK imprime [OK] ou [FALHOU]; o programa retorna 1 se algum falhar.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../Report.hpp"

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

static const char* PAGE = "test_report.html";

static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

static size_t count(const std::string& text, const std::string& part) {
    size_t n = 0;
    for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) n++;
    return n;
}

static ExperimentResult example(const std::string& mode, uint32_t base_pe) {
    ExperimentResult r;
    r.mode = mode;
    r.mode_name = "Modo " + mode;
    r.failed = true;
    r.failure_write = 10000;
    r.load_writes = 2320;
    r.failed_block = 5;
    r.write_amplification = 1.5;
    r.pe.assign(TOTAL_BLOCKS, base_pe);
    r.pe[5] = MAX_PE_CYCLES;
    r.bad_blocks = {5};
    r.wear = summarize(r.pe, 1);
    r.timeline = {{7680, 0, 0, 0}, {10000, 2.5, 0.25, 1000}};
    return r;
}

int main() {
    std::cout << "=== Teste da tela de resultados ===" << std::endl;

    std::cout << "Dados em JSON" << std::endl;
    ScenarioResults hot;
    hot.workload = WorkloadKind::HOT;
    hot.results = {example("direto", 0), example("estatico", 900)};
    std::string json = report_json({hot}, "teste", "09/11/2026 10:00");
    CHECK(json.front() == '{' && json.back() == '}', "objeto JSON completo");
    CHECK(has(json, "\"gerado\":\"09/11/2026 10:00\"") && has(json, "\"comando\":\"teste\""), "data e comando");
    CHECK(has(json, "\"blocos\":128") && has(json, "\"ciclos_max\":1000"), "geometria do dispositivo");
    CHECK(has(json, "\"chave\":\"quente\"") && has(json, "\"chave\":\"estatico\""), "cenario e modos");
    CHECK(has(json, "\"ruins\":[5]") && has(json, "\"evolucao\":[[7680,0,0,0],[10000,2.5,0.25,1000]]"),
          "bad blocks e evolucao");
    CHECK(count(json, "\"pe\":[") == 2 && has(json, "\"wa\":1.5"), "ciclos por bloco e numero com ponto decimal");

    std::cout << "Texto seguro dentro da pagina" << std::endl;
    CHECK(report_detail::text("a\"b\\c") == "\"a\\\"b\\\\c\"", "aspas e barra invertida escapadas");
    CHECK(report_detail::text("</script>") == "\"\\u003c/script>\"", "'<' nao fecha o script da pagina");
    CHECK(report_detail::number(std::nan("")) == "0", "NaN vira 0");

    std::cout << "Arquivo resultados.html" << std::endl;
    ScenarioResults fs;
    fs.workload = WorkloadKind::FILESYSTEM;
    fs.results = {example("mapeamento", 100)};
    CHECK(write_report(PAGE, {fs, hot}, "teste"), "grava a pagina");
    std::ifstream in(PAGE, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string page = buffer.str();
    CHECK(page.rfind("<!doctype html>", 0) == 0 && has(page, "</html>"), "pagina HTML completa");
    CHECK(has(page, "const DADOS = {\"gerado\":"), "dados inseridos no lugar certo");
    CHECK(has(page, "\"chave\":\"fs\"") && has(page, "\"chave\":\"quente\""), "os dois cenarios na mesma pagina");
    CHECK(!has(page, "https://") && count(page, "http://") == count(page, "http://www.w3.org/2000/svg"),
          "nenhum recurso externo (funciona sem internet)");
    CHECK(count(page, "<script") == 2 && !has(page, "<script src"), "scripts embutidos na propria pagina");
    in.close();
    std::remove(PAGE);

    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
