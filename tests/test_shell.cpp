// Teste do shell interativo (Etapa 8): executa roteiros de comandos e
// confere a saída. Cada CHECK imprime [OK] ou [FALHOU].
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include "../Shell.hpp"

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

static const char* DATA = "test_shell_device.bin";
static const char* META = "test_shell_meta.bin";
static const char* CONFIG = "test_shell_config.txt";

static std::string session(const std::string& script) {
    Shell shell(DATA, META, CONFIG);
    std::istringstream in(script);
    std::ostringstream out;
    shell.run(in, out);
    return out.str();
}

static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

int main() {
    std::cout << "=== Teste do shell interativo ===" << std::endl;
    std::remove(DATA);
    std::remove(META);
    std::remove(CONFIG);

    std::cout << "Primeira sessao" << std::endl;
    std::string out = session(
        "formatar mapeamento\n"
        "criar notas.txt\n"
        "escrever notas.txt Ola pendrive\n"
        "ler notas.txt\n"
        "ls\n"
        "carga 300\n"
        "estatisticas\n"
        "formatar banana\n"
        "xyz\n"
        "sair\n");
    CHECK(has(out, "Nenhum dispositivo encontrado"), "sem arquivos, cria um dispositivo novo");
    CHECK(has(out, "formatado no modo 'mapeamento'"), "formatar mapeamento");
    CHECK(has(out, "Ola pendrive"), "escrever e ler um arquivo");
    CHECK(has(out, "notas.txt") && has(out, "12 bytes"), "ls mostra o arquivo e o tamanho");
    CHECK(has(out, "copiando fotos e musicas") && has(out, "[OK] 300 operacoes"), "carga com preparacao e 300 operacoes");
    CHECK(has(out, "Modo: Mapeamento de paginas sem wear leveling") && has(out, "Garbage collection"),
          "estatisticas do modo com mapeamento");
    CHECK(has(out, "Modo invalido"), "modo invalido e recusado");
    CHECK(has(out, "Comando desconhecido"), "comando desconhecido");
    CHECK(has(out, "Desgaste salvo"), "sair salva o desgaste");

    std::cout << "Segunda sessao (reabre o mesmo dispositivo)" << std::endl;
    out = session("modo\nler notas.txt\napagar notas.txt\nler notas.txt\ndesgaste\n");
    CHECK(has(out, "carregado no modo 'mapeamento'"), "modo lembrado entre sessoes");
    CHECK(has(out, "Ola pendrive"), "arquivo continua la");
    CHECK(has(out, "apagado") && has(out, "NOT_FOUND"), "apagar e conferir");
    CHECK(has(out, "Ciclos P/E por bloco") && has(out, "112:"), "mapa de desgaste dos 128 blocos");

    std::cout << "Terceira sessao (wear leveling estatico)" << std::endl;
    out = session("formatar estatico\ncarga 200\nestatisticas\nsair\n");
    CHECK(has(out, "Wear leveling estatico:"), "estatisticas mostram as migracoes do estatico");

    std::remove(DATA);
    std::remove(META);
    std::remove(CONFIG);
    std::cout << (failures == 0 ? "\nTodos os testes passaram." : "\nHa testes com falha.") << std::endl;
    return failures == 0 ? 0 : 1;
}
