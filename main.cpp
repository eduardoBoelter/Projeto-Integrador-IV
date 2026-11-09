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
#include "Shell.hpp"
#include "Report.hpp"

// Uso:
//   ./simulador                    shell interativo sobre o pendrive simulado (Etapa 8):
//                                  nand_device.bin, nand_meta.bin e nand_config.txt
//   ./simulador < comandos.txt     o mesmo shell, lendo os comandos de um arquivo
//   ./simulador --demo             demonstração da FTL com mapeamento (arquivos próprios)
//   ./simulador --comparar         roda as duas cargas até a 1ª falha nos quatro modos da FTL,
//                                  exporta os CSVs e abre a tela de resultados (resultados.html)
//   ./simulador --cenario-quente   só a carga de dados quentes
//   ./simulador --cenario-fs       só a carga do sistema de arquivos
//   Acrescente --sem-abrir para gravar resultados.html sem abrir o navegador.
//   Os experimentos usam arquivos próprios e não mexem no dispositivo do shell.

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
    NANDFlashMemory flash("demo_device.bin", "demo_meta.bin");
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

int main(int argc, char* argv[]) {
    const std::string option = argc > 1 ? argv[1] : "";
    const bool open = !(argc > 2 && std::string(argv[2]) == "--sem-abrir");
    if (option == "--comparar")
        return run_comparison({WorkloadKind::FILESYSTEM, WorkloadKind::HOT}, "./simulador --comparar", open, std::cout);
    if (option == "--cenario-fs")
        return run_comparison({WorkloadKind::FILESYSTEM}, "./simulador --cenario-fs", open, std::cout);
    if (option == "--cenario-quente")
        return run_comparison({WorkloadKind::HOT}, "./simulador --cenario-quente", open, std::cout);
    if (option == "--demo") {
        std::cout << "=== Simulador de Memoria NAND Flash ===" << std::endl;
        return run_demo(true);
    }
    if (!option.empty()) {
        std::cerr << "Opcao desconhecida: " << option << "\nUse: ./simulador [--demo | --comparar | --cenario-fs | --cenario-quente] [--sem-abrir]" << std::endl;
        return 1;
    }

    std::cout << "=== Simulador de pendrive com memoria NAND Flash ===" << std::endl;
    Shell shell;
    shell.run(std::cin, std::cout);
    return 0;
}
