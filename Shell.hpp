#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Metrics.hpp"
#include "Workload.hpp"

// ---------------------------------------------------------------------------
// Etapa 8: shell interativo.
//
// Usa o pendrive simulado como um usuário usaria: formatar, criar, gravar,
// ler e apagar arquivos, e acompanhar o desgaste dos blocos. O modo da FTL é
// escolhido na formatação e guardado em nand_config.txt, porque cada modo
// organiza os dados de um jeito e não dá para trocar sem formatar.
//
// Os comandos podem vir do teclado ou de um arquivo:
//     ./simulador < comandos.txt
// ---------------------------------------------------------------------------

// Modos da FTL que o shell conhece
enum class FTLMode { DIRECT, MAPPING, WL_DYNAMIC, WL_STATIC };

inline const char* mode_key(FTLMode mode) {
    switch (mode) {
        case FTLMode::DIRECT:     return "direto";
        case FTLMode::MAPPING:    return "mapeamento";
        case FTLMode::WL_DYNAMIC: return "dinamico";
        case FTLMode::WL_STATIC:  return "estatico";
    }
    return "?";
}

inline bool parse_mode(const std::string& text, FTLMode& mode) {
    for (FTLMode m : {FTLMode::DIRECT, FTLMode::MAPPING, FTLMode::WL_DYNAMIC, FTLMode::WL_STATIC}) {
        if (text == mode_key(m)) { mode = m; return true; }
    }
    return false;
}

inline std::unique_ptr<FTL> make_ftl(FTLMode mode, IODriver& driver) {
    switch (mode) {
        case FTLMode::DIRECT:     return std::make_unique<DirectFTL>(driver);
        case FTLMode::MAPPING:    return std::make_unique<PageMappingFTL>(driver);
        case FTLMode::WL_DYNAMIC: return std::make_unique<WearLevelingFTL>(driver, false);
        case FTLMode::WL_STATIC:  return std::make_unique<WearLevelingFTL>(driver, true);
    }
    return nullptr;
}

class Shell {
private:
    std::string config_file;
    NANDFlashMemory flash;
    std::unique_ptr<IODriver> driver;
    std::unique_ptr<FTL> ftl;
    std::unique_ptr<SimpleFS> fs;
    FTLMode mode = FTLMode::WL_STATIC;
    uint32_t workload_seed = 1;

    void save_mode() const {
        std::ofstream out(config_file, std::ios::trunc);
        out << mode_key(mode) << "\n";
    }

    bool load_mode() {
        std::ifstream in(config_file);
        std::string word;
        return in >> word && parse_mode(word, mode);
    }

    // (Re)cria driver, FTL e sistema de arquivos sobre a camada física
    void attach_layers() {
        fs.reset();
        ftl.reset();
        driver = std::make_unique<IODriver>(flash);
        ftl = make_ftl(mode, *driver);
        fs = std::make_unique<SimpleFS>(*ftl);
    }

    bool format_device(FTLMode new_mode, std::ostream& out) {
        mode = new_mode;
        if (!flash.format()) {
            out << "[ERRO] Falha ao formatar o dispositivo." << std::endl;
            return false;
        }
        attach_layers();
        save_mode();
        if (fs->format() != FSResult::OK) {
            out << "[ERRO] Falha ao criar o sistema de arquivos." << std::endl;
            return false;
        }
        flash.save_metadata();
        out << "[OK] Dispositivo formatado no modo '" << mode_key(mode) << "' (" << ftl->name()
            << "). Desgaste zerado." << std::endl;
        return true;
    }

    // ----------------------------- comandos -----------------------------

    void cmd_help(std::ostream& out) const {
        out << "Comandos:\n"
            << "  formatar <modo>        apaga tudo e zera o desgaste; modos: direto, mapeamento, dinamico, estatico\n"
            << "  modo                   mostra o modo atual da FTL\n"
            << "  ls                     lista os arquivos\n"
            << "  criar <nome>           cria um arquivo vazio\n"
            << "  escrever <nome> <txt>  grava o texto no arquivo (substitui o conteudo)\n"
            << "  ler <nome>             mostra o conteudo do arquivo\n"
            << "  apagar <nome>          apaga o arquivo\n"
            << "  carga <n>              executa n operacoes de uso de pendrive (editar, criar, apagar)\n"
            << "  estatisticas           escritas, amplificacao, GC e resumo do desgaste\n"
            << "  desgaste               ciclos P/E de cada um dos 128 blocos\n"
            << "  exportar <arquivo.csv> exporta os ciclos P/E dos blocos em CSV\n"
            << "  ajuda                  mostra esta lista\n"
            << "  sair                   salva e encerra" << std::endl;
    }

    void cmd_ls(std::ostream& out) const {
        auto files = fs->list();
        if (files.empty()) out << "(nenhum arquivo)" << std::endl;
        for (const auto& f : files)
            out << "  " << std::left << std::setw(24) << f.name << std::right << std::setw(10) << f.size << " bytes"
                << std::endl;
        out << files.size() << " arquivo(s), " << fs->free_bytes() / 1024 << " KB livres" << std::endl;
    }

    void cmd_stats(std::ostream& out) const {
        const IOStats& io = driver->stats();
        const FTLStats& fst = ftl->stats();
        WearSummary w = summarize(*driver);
        out << std::fixed << std::setprecision(2)
            << "Modo: " << ftl->name() << "\n"
            << "Nesta sessao: escritas logicas " << fst.host_writes << " | paginas programadas " << io.page_programs
            << " | apagamentos " << io.block_erases << " | amplificacao " << ftl->write_amplification() << "\n"
            << "Desgaste acumulado: media " << w.mean << " | desvio padrao " << w.stdev << " | CV " << w.cv
            << " | min " << w.min << " | max " << w.max << " de " << MAX_PE_CYCLES << " | bad blocks " << w.bad
            << "\n"
            << "Vida usada: " << 100 * w.life_used << "% da vida total dos blocos | blocos nunca apagados: "
            << w.untouched << std::endl;
        if (auto* mapping = dynamic_cast<const PageMappingFTL*>(ftl.get()))
            out << "Garbage collection: " << mapping->gc().runs << " execucoes, " << mapping->gc().pages_copied
                << " paginas copiadas" << std::endl;
        if (auto* wl = dynamic_cast<const WearLevelingFTL*>(ftl.get()); wl && wl->static_enabled())
            out << "Wear leveling estatico: " << wl->wl().static_migrations << " migracoes de dados frios" << std::endl;
        out << std::defaultfloat;
    }

    void cmd_export(const std::string& file, std::ostream& out) const {
        if (export_wear_csv(file, {mode_key(mode)}, {pe_counts(*driver)}))
            out << "[OK] Ciclos P/E dos 128 blocos exportados em " << file << std::endl;
        else
            out << "[ERRO] Nao foi possivel criar " << file << std::endl;
    }

    void cmd_wear(std::ostream& out) const {
        out << "Ciclos P/E por bloco (X = bad block):" << std::endl;
        for (uint32_t row = 0; row < TOTAL_BLOCKS / 16; ++row) {
            out << "  " << std::setw(3) << row * 16 << ":";
            for (uint32_t col = 0; col < 16; ++col) {
                const NANDBlock& block = driver->block_info(row * 16 + col);
                if (block.is_bad_block) out << std::setw(5) << "X";
                else out << std::setw(5) << block.pe_cycles;
            }
            out << std::endl;
        }
    }

    void cmd_load(uint64_t steps, std::ostream& out) {
        FSWorkload load(*fs, workload_seed++);
        FSResult r = FSResult::OK;
        if (load.needs_preparation()) {
            out << "Disco quase vazio: copiando fotos e musicas (dados frios)..." << std::endl;
            r = load.prepare();
        }
        uint64_t done = 0;
        while (r == FSResult::OK && done < steps) {
            r = load.step();
            if (r == FSResult::OK) done++;
        }
        const WorkloadStats& w = load.stats();
        out << "[" << (r == FSResult::OK ? "OK" : "PAROU") << "] " << done << " operacoes (" << w.edits
            << " edicoes, " << w.creates << " criacoes, " << w.removes << " remocoes)";
        if (r != FSResult::OK) out << " | motivo: " << to_string(r);
        out << std::endl;
        if (ftl->stats().first_failure_write != 0)
            out << "[AVISO] Primeira falha de bloco: bloco " << ftl->stats().first_failed_block
                << " na escrita logica " << ftl->stats().first_failure_write << " desta sessao." << std::endl;
    }

    static std::string rest_of_line(std::istringstream& in) {
        std::string text;
        std::getline(in, text);
        size_t start = text.find_first_not_of(' ');
        return start == std::string::npos ? "" : text.substr(start);
    }

    void report(FSResult r, const std::string& what, std::ostream& out) const {
        if (r == FSResult::OK) out << "[OK] " << what << std::endl;
        else out << "[ERRO] " << to_string(r) << std::endl;
    }

public:
    Shell(const std::string& data_file = "nand_device.bin", const std::string& meta_file = "nand_meta.bin",
          const std::string& config = "nand_config.txt")
        : config_file(config), flash(data_file, meta_file) {}

    // Abre o dispositivo existente ou cria um novo no modo com wear leveling
    bool start(std::ostream& out) {
        OpenResult opened = flash.open();
        if (opened == OpenResult::OK && load_mode()) {
            attach_layers();
            FSResult mounted = fs->mount();
            out << "[OK] Dispositivo carregado no modo '" << mode_key(mode) << "' (desgaste preservado)." << std::endl;
            if (mounted != FSResult::OK)
                out << "[AVISO] Sistema de arquivos nao encontrado. Use 'formatar <modo>'." << std::endl;
            return true;
        }
        out << "Nenhum dispositivo encontrado: criando um novo." << std::endl;
        return format_device(FTLMode::WL_STATIC, out);
    }

    // Executa um comando. Retorna false para "sair".
    bool execute(const std::string& line, std::ostream& out) {
        std::istringstream in(line);
        std::string cmd;
        if (!(in >> cmd)) return true;
        std::string name;

        if (cmd == "sair" || cmd == "exit") {
            flash.save_metadata();
            out << "[OK] Desgaste salvo em " << flash.meta_file() << ". Ate mais!" << std::endl;
            return false;
        } else if (cmd == "ajuda" || cmd == "help") {
            cmd_help(out);
        } else if (cmd == "formatar") {
            std::string word;
            FTLMode new_mode = mode;
            if (in >> word && !parse_mode(word, new_mode)) {
                out << "[ERRO] Modo invalido. Use: direto, mapeamento, dinamico ou estatico." << std::endl;
                return true;
            }
            format_device(new_mode, out);
        } else if (cmd == "modo") {
            out << "Modo atual: " << mode_key(mode) << " (" << ftl->name() << ")" << std::endl;
        } else if (cmd == "ls") {
            cmd_ls(out);
        } else if (cmd == "criar" && in >> name) {
            report(fs->create(name), "Arquivo '" + name + "' criado.", out);
        } else if (cmd == "escrever" && in >> name) {
            std::string text = rest_of_line(in);
            report(fs->write_file(name, std::vector<uint8_t>(text.begin(), text.end())),
                   std::to_string(text.size()) + " bytes gravados em '" + name + "'.", out);
        } else if (cmd == "ler" && in >> name) {
            std::vector<uint8_t> content;
            FSResult r = fs->read_file(name, content);
            if (r == FSResult::OK) out << std::string(content.begin(), content.end()) << std::endl;
            else report(r, "", out);
        } else if (cmd == "apagar" && in >> name) {
            report(fs->remove(name), "Arquivo '" + name + "' apagado.", out);
        } else if (cmd == "carga") {
            uint64_t steps = 1000;
            in >> steps;
            cmd_load(steps, out);
        } else if (cmd == "estatisticas" || cmd == "stats") {
            cmd_stats(out);
        } else if (cmd == "desgaste") {
            cmd_wear(out);
        } else if (cmd == "exportar" && in >> name) {
            cmd_export(name, out);
        } else {
            out << "Comando desconhecido ou incompleto: '" << line << "'. Digite 'ajuda'." << std::endl;
            return true;
        }
        flash.save_metadata();
        return true;
    }

    // Laço principal: lê comandos até "sair" ou fim da entrada
    void run(std::istream& in, std::ostream& out) {
        if (!start(out)) return;
        out << "Digite 'ajuda' para ver os comandos." << std::endl;
        std::string line;
        while (true) {
            out << "nand> " << std::flush;
            if (!std::getline(in, line)) {
                out << std::endl;
                flash.save_metadata();
                break;
            }
            if (!execute(line, out)) break;
        }
    }

    FTLMode current_mode() const { return mode; }
    const IODriver& io() const { return *driver; }
};
