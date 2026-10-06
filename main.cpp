#include <iostream>
#include "NANDFlash.hpp"

int main() {
    std::cout << "=== Inicializando Simulador de Memoria NAND Flash ===" << std::endl;

    NANDFlashMemory flash("nand_device.bin");
    if (flash.initialize_storage()) {
        std::cout << "[OK] Arquivo binario de armazenamento inicializado com sucesso!" << std::endl;
    } else {
        std::cerr << "[ERRO] Falha ao criar arquivo de armazenamento binario." << std::endl;
        return 1;
    }

    std::cout << "Total de blocos criados: " << flash.get_total_blocks() << std::endl;
    std::cout << "Paginas por bloco: " << PAGES_PER_BLOCK << std::endl;
    std::cout << "Tamanho de cada pagina: " << PAGE_SIZE << " bytes" << std::endl;
    std::cout << "Ciclo P/E inicial do Bloco 0: " << flash.blocks[0].pe_cycles << std::endl;

    // Teste de apagamento
    if (flash.erase_block(0) != EraseResult::OK) {
        std::cerr << "[ERRO] Falha ao apagar o Bloco 0." << std::endl;
        return 1;
    }
    std::cout << "Ciclo P/E do Bloco 0 apos 1 apagamento: " << flash.blocks[0].pe_cycles << std::endl;

    // Teste de fim de vida: apaga o Bloco 1 ate atingir o limite de P/E
    EraseResult result = EraseResult::OK;
    while (result == EraseResult::OK) {
        result = flash.erase_block(1);
    }
    std::cout << "Bloco 1 virou bad block apos " << flash.blocks[1].pe_cycles << " ciclos P/E: "
              << (result == EraseResult::WORN_OUT ? "sim" : "nao") << std::endl;
    std::cout << "Novo apagamento no Bloco 1 e recusado: "
              << (flash.erase_block(1) == EraseResult::BAD_BLOCK ? "sim" : "nao") << std::endl;

    return 0;
}
