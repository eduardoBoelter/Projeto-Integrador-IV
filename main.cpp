#include <iostream>
#include <string>
#include "NANDFlash.hpp"

// Uso: ./simulador            abre o dispositivo existente (ou formata, se não houver)
//      ./simulador --format   força a formatação, zerando o desgaste
int main(int argc, char* argv[]) {
    std::cout << "=== Simulador de Memoria NAND Flash ===" << std::endl;

    const bool force_format = (argc > 1 && std::string(argv[1]) == "--format");
    NANDFlashMemory flash("nand_device.bin", "nand_meta.bin");

    OpenResult opened = force_format ? OpenResult::NO_DEVICE : flash.open();
    if (opened == OpenResult::OK) {
        std::cout << "[OK] Dispositivo existente carregado (desgaste preservado)." << std::endl;
    } else {
        if (opened == OpenResult::INVALID_FORMAT || opened == OpenResult::GEOMETRY_MISMATCH) {
            std::cout << "[AVISO] Metadados invalidos ou de outra geometria; formatando." << std::endl;
        }
        if (!flash.format()) {
            std::cerr << "[ERRO] Falha ao formatar o dispositivo." << std::endl;
            return 1;
        }
        std::cout << "[OK] Novo dispositivo formatado (" << flash.data_file() << " e "
                  << flash.meta_file() << ")." << std::endl;
    }

    std::cout << "Blocos: " << flash.get_total_blocks()
              << " | Paginas por bloco: " << PAGES_PER_BLOCK
              << " | Tamanho da pagina: " << PAGE_SIZE << " bytes"
              << " | Limite P/E: " << MAX_PE_CYCLES << std::endl;
    std::cout << "Ciclo P/E do Bloco 0 ao abrir: " << flash.blocks[0].pe_cycles << std::endl;

    // Um apagamento por execução: rodar o programa várias vezes mostra o
    // contador crescendo, o que comprova que o desgaste é persistido.
    EraseResult result = flash.erase_block(0);
    if (result == EraseResult::IO_ERROR) {
        std::cerr << "[ERRO] Falha ao apagar o Bloco 0." << std::endl;
        return 1;
    }
    std::cout << "Ciclo P/E do Bloco 0 apos apagamento: " << flash.blocks[0].pe_cycles
              << (result == EraseResult::BAD_BLOCK ? " (bad block, apagamento recusado)" : "")
              << std::endl;

    std::cout << "Apagamentos totais: " << flash.total_erases()
              << " | Bad blocks: " << flash.bad_block_count() << std::endl;

    if (!flash.save_metadata()) {
        std::cerr << "[ERRO] Falha ao salvar os metadados." << std::endl;
        return 1;
    }
    std::cout << "[OK] Metadados salvos em " << flash.meta_file() << "." << std::endl;
    return 0;
}
