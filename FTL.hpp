#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "IODriver.hpp"

// Blocos reservados para a FTL (over-provisioning). Não fazem parte da
// capacidade lógica, para os três modos terem o mesmo tamanho visível.
constexpr size_t OVERPROVISION_BLOCKS = 8;
constexpr size_t LOGICAL_BLOCKS = TOTAL_BLOCKS - OVERPROVISION_BLOCKS; // 120
constexpr uint32_t LOGICAL_PAGES = static_cast<uint32_t>(LOGICAL_BLOCKS * PAGES_PER_BLOCK); // 7680 LBAs

// Resultado das operações lógicas (o que o sistema de arquivos enxerga)
enum class FTLResult : uint8_t {
    OK = 0,
    OUT_OF_RANGE = 1,     // LBA além da capacidade lógica
    DEVICE_WORN_OUT = 2,  // Um bloco necessário chegou ao fim da vida: o dado não pôde ser gravado
    IO_ERROR = 3          // Falha de acesso ao arquivo
};

// Contadores do ponto de vista do sistema de arquivos (escritas "lógicas")
struct FTLStats {
    uint64_t host_writes = 0;          // escritas pedidas pela camada de cima
    uint64_t host_reads = 0;           // leituras pedidas pela camada de cima
    uint64_t first_failure_write = 0;  // nº da escrita lógica em que o 1º bloco virou bad block (0 = ainda não)
    uint32_t first_failed_block = 0;   // qual bloco falhou primeiro
};

// Interface comum aos três modos da FTL (Etapas 4, 5 e 6).
// O sistema de arquivos só conhece esta interface: read(lba) e write(lba).
// Por isso, o mesmo cenário de carga roda em qualquer modo sem alteração.
class FTL {
protected:
    IODriver& driver;
    FTLStats ftl_stats;

    // Registra o momento da primeira falha de bloco (métrica da pesquisa)
    void note_erase(EraseResult result, uint32_t block_id) {
        if (result == EraseResult::WORN_OUT && ftl_stats.first_failure_write == 0) {
            ftl_stats.first_failure_write = ftl_stats.host_writes;
            ftl_stats.first_failed_block = block_id;
        }
    }

public:
    explicit FTL(IODriver& io) : driver(io) {}
    virtual ~FTL() = default;

    virtual FTLResult write(uint32_t lba, const uint8_t* data) = 0;
    virtual FTLResult read(uint32_t lba, uint8_t* buffer) = 0;
    virtual const char* name() const = 0;

    uint32_t logical_capacity() const { return LOGICAL_PAGES; }
    const FTLStats& stats() const { return ftl_stats; }
    const IODriver& io() const { return driver; }

    // Amplificação de escrita: páginas gravadas fisicamente / escritas pedidas.
    // Vale 1,0 no caso ideal; acima disso, a FTL está regravando dados extras.
    double write_amplification() const {
        if (ftl_stats.host_writes == 0) return 0.0;
        return static_cast<double>(driver.stats().page_programs) /
               static_cast<double>(ftl_stats.host_writes);
    }
};

// ---------------------------------------------------------------------------
// Etapa 4: FTL com mapeamento direto (LBA = PBA), o cenário SEM nivelamento.
//
// O LBA vira endereço físico por divisão:
//     bloco  = lba / PAGES_PER_BLOCK
//     página = lba % PAGES_PER_BLOCK
// Como a Flash não permite sobrescrever, alterar um LBA já gravado exige o
// ciclo read-modify-erase-write: ler as páginas válidas do bloco para a RAM,
// apagar o bloco inteiro e regravar tudo com o dado novo no lugar.
// Resultado: cada alteração custa 1 apagamento no MESMO bloco, e os blocos
// que guardam dados quentes (metadados) se desgastam muito antes dos outros.
// ---------------------------------------------------------------------------
class DirectFTL : public FTL {
private:
    static PhysicalAddress to_physical(uint32_t lba) {
        return {lba / static_cast<uint32_t>(PAGES_PER_BLOCK), lba % static_cast<uint32_t>(PAGES_PER_BLOCK)};
    }

    // Converte o resultado de uma operação do driver em resultado da FTL
    static FTLResult from_io(IOResult result) {
        switch (result) {
            case IOResult::OK:        return FTLResult::OK;
            case IOResult::BAD_BLOCK: return FTLResult::DEVICE_WORN_OUT;
            default:                  return FTLResult::IO_ERROR;
        }
    }

    // Altera um LBA já gravado: read-modify-erase-write do bloco inteiro
    FTLResult rewrite_block(PhysicalAddress target, const uint8_t* data, uint32_t lba) {
        const NANDBlock& info = driver.block_info(target.block);

        // 1. Copia para a RAM todas as páginas válidas do bloco (com seus LBAs)
        std::vector<uint8_t> saved(PAGES_PER_BLOCK * PAGE_SIZE);
        std::vector<uint32_t> saved_lba(PAGES_PER_BLOCK, INVALID_LBA);
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; ++p) {
            if (info.pages[p].state != PageState::VALID) continue;
            saved_lba[p] = info.pages[p].logical_address;
            if (p == target.page) continue; // esta será substituída pelo dado novo
            if (driver.read_page({target.block, p}, &saved[p * PAGE_SIZE]) != IOResult::OK)
                return FTLResult::IO_ERROR;
        }

        // 2. Modifica, na RAM, a página que mudou
        std::memcpy(&saved[target.page * PAGE_SIZE], data, PAGE_SIZE);
        saved_lba[target.page] = lba;

        // 3. Apaga o bloco inteiro (é aqui que o desgaste acontece)
        EraseResult erased = driver.erase_block(target.block);
        note_erase(erased, target.block);
        if (erased == EraseResult::IO_ERROR) return FTLResult::IO_ERROR;
        if (erased != EraseResult::OK) {
            // O bloco morreu neste apagamento: no mapeamento direto não existe
            // outro lugar para o LBA, então os dados do bloco são perdidos.
            return FTLResult::DEVICE_WORN_OUT;
        }

        // 4. Regrava todas as páginas que estavam válidas
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; ++p) {
            if (saved_lba[p] == INVALID_LBA) continue;
            IOResult programmed = driver.program_page({target.block, p}, &saved[p * PAGE_SIZE], saved_lba[p]);
            if (programmed != IOResult::OK) return from_io(programmed);
        }
        return FTLResult::OK;
    }

public:
    explicit DirectFTL(IODriver& io) : FTL(io) {}

    const char* name() const override { return "Mapeamento direto (LBA = PBA)"; }

    FTLResult write(uint32_t lba, const uint8_t* data) override {
        if (lba >= LOGICAL_PAGES) return FTLResult::OUT_OF_RANGE;
        ftl_stats.host_writes++;

        PhysicalAddress pba = to_physical(lba);
        if (driver.block_info(pba.block).is_bad_block) return FTLResult::DEVICE_WORN_OUT;

        // Primeira gravação do LBA: a página ainda está livre, basta programar
        if (driver.page_info(pba).state == PageState::FREE) {
            return from_io(driver.program_page(pba, data, lba));
        }
        // LBA já gravado: não há sobrescrita, então o bloco inteiro é refeito
        return rewrite_block(pba, data, lba);
    }

    FTLResult read(uint32_t lba, uint8_t* buffer) override {
        if (lba >= LOGICAL_PAGES) return FTLResult::OUT_OF_RANGE;
        ftl_stats.host_reads++;

        PhysicalAddress pba = to_physical(lba);
        // LBA nunca gravado (ou perdido num bloco que morreu): retorna "apagado"
        if (driver.page_info(pba).state != PageState::VALID) {
            std::memset(buffer, ERASED_BYTE, PAGE_SIZE);
            return FTLResult::OK;
        }
        return driver.read_page(pba, buffer) == IOResult::OK ? FTLResult::OK : FTLResult::IO_ERROR;
    }
};

inline const char* to_string(FTLResult result) {
    switch (result) {
        case FTLResult::OK:              return "OK";
        case FTLResult::OUT_OF_RANGE:    return "OUT_OF_RANGE";
        case FTLResult::DEVICE_WORN_OUT: return "DEVICE_WORN_OUT";
        case FTLResult::IO_ERROR:        return "IO_ERROR";
    }
    return "?";
}
