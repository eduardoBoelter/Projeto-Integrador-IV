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
    flash.blocks[0].erase();
    std::cout << "Ciclo P/E do Bloco 0 apos 1 apagamento: " << flash.blocks[0].pe_cycles << std::endl;

    return 0;
}
