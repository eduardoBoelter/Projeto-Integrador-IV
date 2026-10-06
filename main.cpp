#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "NANDFlash.hpp"
#include "IODriver.hpp"

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

    IODriver driver(flash);
    const PhysicalAddress pba{0, 0};

    // Se a página (0, 0) já foi usada numa execução anterior, é preciso apagar
    // o bloco inteiro antes de gravar de novo: na Flash não existe sobrescrita.
    if (driver.page_info(pba).state != PageState::FREE) {
        std::cout << "\nPagina (0, 0) ja foi usada; apagando o Bloco 0 antes de gravar." << std::endl;
        if (driver.erase_block(0) != EraseResult::OK) {
            std::cerr << "[ERRO] Nao foi possivel apagar o Bloco 0." << std::endl;
            return 1;
        }
    }
    std::cout << "Ciclo P/E do Bloco 0: " << driver.block_info(0).pe_cycles << std::endl;

    // 1. Grava uma mensagem na página (0, 0), associada ao LBA 0
    std::vector<uint8_t> data(PAGE_SIZE, ERASED_BYTE);
    const std::string message = "Ola, NAND! Execucao com P/E " + std::to_string(driver.block_info(0).pe_cycles);
    std::memcpy(data.data(), message.c_str(), message.size() + 1);
    std::cout << "\nprogram_page(0, 0, LBA 0): " << to_string(driver.program_page(pba, data.data(), 0)) << std::endl;

    // 2. Lê de volta
    std::vector<uint8_t> buffer(PAGE_SIZE);
    driver.read_page(pba, buffer.data());
    std::cout << "read_page(0, 0): \"" << reinterpret_cast<const char*>(buffer.data()) << "\"" << std::endl;

    // 3. Tenta sobrescrever: o driver recusa
    std::cout << "program_page(0, 0) de novo: " << to_string(driver.program_page(pba, data.data(), 0))
              << " (sobrescrita proibida)" << std::endl;

    // 4. Invalida a página, como a FTL fará quando o dado for atualizado em outro lugar
    std::cout << "invalidate_page(0, 0): " << to_string(driver.invalidate_page(pba)) << std::endl;

    const NANDBlock& block = driver.block_info(0);
    std::cout << "\nBloco 0: " << block.free_pages_count << " livres, " << block.valid_pages_count
              << " validas, " << block.invalid_pages_count << " invalidas" << std::endl;

    const IOStats& s = driver.stats();
    std::cout << "Operacoes: " << s.page_reads << " leitura(s), " << s.page_programs << " programacao(oes), "
              << s.page_invalidations << " invalidacao(oes), " << s.block_erases << " apagamento(s), "
              << s.rejected_operations << " recusada(s)" << std::endl;

    if (!flash.save_metadata()) {
        std::cerr << "[ERRO] Falha ao salvar os metadados." << std::endl;
        return 1;
    }
    std::cout << "[OK] Metadados salvos em " << flash.meta_file() << "." << std::endl;
    return 0;
}
