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

// ---------------------------------------------------------------------------
// Etapa 5: FTL com mapeamento de páginas (LBA -> PBA) e garbage collection.
//
// Escrita fora do lugar (out-of-place): toda gravação vai para a próxima
// página livre do "bloco ativo"; a página antiga do LBA vira inválida e a
// tabela de mapeamento passa a apontar para a nova. Nenhuma escrita apaga
// nada diretamente.
//
// Quando sobram poucos blocos livres, o garbage collection escolhe uma
// vítima, copia as páginas válidas dela para o bloco ativo e a apaga,
// devolvendo-a à lista de blocos livres.
//
// Nesta etapa o bloco livre escolhido é sempre o de MENOR NÚMERO (política
// ingênua, sem nivelamento). A Etapa 6 troca só essa escolha, em
// select_free_block(), pelo bloco de menor contador P/E (wear leveling).
// ---------------------------------------------------------------------------

// Contadores próprios do garbage collection
struct GCStats {
    uint64_t runs = 0;          // quantas vezes o GC apagou uma vítima
    uint64_t pages_copied = 0;  // páginas válidas copiadas pelo GC
};

class PageMappingFTL : public FTL {
public:
    // Abaixo deste número de blocos livres, o GC é acionado antes de abrir um novo bloco ativo
    static constexpr size_t GC_MIN_FREE_BLOCKS = 2;

private:
    static constexpr uint32_t NO_BLOCK = 0xFFFFFFFF;

    std::vector<PhysicalAddress> l2p;   // tabela de mapeamento: LBA -> PBA
    std::vector<bool> mapped;           // o LBA já foi gravado?
    std::vector<bool> is_free;          // bloco totalmente livre, disponível para uso
    size_t free_count = 0;
    uint32_t active_block = NO_BLOCK;   // bloco que está recebendo as escritas
    uint32_t next_page = 0;             // próxima página livre do bloco ativo
    GCStats gc_stats;

    // Reconstrói a tabela de mapeamento a partir dos metadados das páginas
    // (estado e LBA), que a camada física guarda em nand_meta.bin. Assim, a
    // FTL funciona tanto num dispositivo novo quanto num já usado.
    void rebuild() {
        l2p.assign(LOGICAL_PAGES, PhysicalAddress{0, 0});
        mapped.assign(LOGICAL_PAGES, false);
        is_free.assign(TOTAL_BLOCKS, false);
        free_count = 0;

        for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
            const NANDBlock& block = driver.block_info(b);
            if (block.is_bad_block) continue;
            if (block.free_pages_count == PAGES_PER_BLOCK) {
                is_free[b] = true;
                free_count++;
                continue;
            }
            uint32_t last_used = 0;
            for (uint32_t p = 0; p < PAGES_PER_BLOCK; ++p) {
                const Page& page = block.pages[p];
                if (page.state != PageState::FREE) last_used = p;
                if (page.state == PageState::VALID && page.logical_address < LOGICAL_PAGES) {
                    l2p[page.logical_address] = {b, p};
                    mapped[page.logical_address] = true;
                }
            }
            // Bloco parcialmente gravado (só páginas livres no final): volta a ser
            // o bloco ativo, para não desperdiçar as páginas que sobraram nele.
            if (active_block == NO_BLOCK && last_used + 1 < PAGES_PER_BLOCK &&
                block.free_pages_count == PAGES_PER_BLOCK - (last_used + 1)) {
                active_block = b;
                next_page = last_used + 1;
            }
        }
    }

    // Retira um bloco da lista de livres e o torna o bloco ativo
    bool open_new_active_block() {
        if (free_count == 0) return false;
        uint32_t chosen = select_free_block();
        is_free[chosen] = false;
        free_count--;
        active_block = chosen;
        next_page = 0;
        return true;
    }

    // Grava os dados na próxima página livre do bloco ativo e atualiza o
    // mapeamento. allow_gc = false quando quem chama é o próprio GC.
    FTLResult append(uint32_t lba, const uint8_t* data, bool allow_gc) {
        if (active_block == NO_BLOCK || next_page >= PAGES_PER_BLOCK) {
            if (allow_gc) {
                // Libera espaço antes de consumir mais um bloco livre
                while (free_count < GC_MIN_FREE_BLOCKS && collect_garbage()) {}
            }
            if (!open_new_active_block()) return FTLResult::DEVICE_WORN_OUT;
        }

        PhysicalAddress target{active_block, next_page++};
        IOResult programmed = driver.program_page(target, data, lba);
        if (programmed != IOResult::OK) return FTLResult::IO_ERROR;

        // A versão anterior do LBA vira inválida (o dado antigo fica no bloco até o GC)
        if (mapped[lba]) driver.invalidate_page(l2p[lba]);
        l2p[lba] = target;
        mapped[lba] = true;
        return FTLResult::OK;
    }

    // Escolha da vítima: política gulosa (greedy), o bloco com mais páginas
    // inválidas, pois é o que libera mais espaço copiando menos.
    uint32_t select_victim() const {
        uint32_t victim = NO_BLOCK;
        size_t most_invalid = 0;
        for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
            if (is_free[b] || b == active_block) continue;
            const NANDBlock& block = driver.block_info(b);
            if (block.is_bad_block) continue;
            if (block.invalid_pages_count > most_invalid) {
                most_invalid = block.invalid_pages_count;
                victim = b;
            }
        }
        return victim;
    }

    // Uma rodada de garbage collection. Retorna false se não há o que coletar.
    bool collect_garbage() {
        uint32_t victim = select_victim();
        if (victim == NO_BLOCK) return false;

        // 1. Copia as páginas ainda válidas da vítima para o bloco ativo
        std::vector<uint8_t> buffer(PAGE_SIZE);
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; ++p) {
            const Page& page = driver.block_info(victim).pages[p];
            if (page.state != PageState::VALID) continue;
            uint32_t lba = page.logical_address;
            // Cópia antiga que a tabela não aponta mais (por exemplo, num dispositivo
            // usado antes por outro modo): não é copiada, será apagada com a vítima.
            if (lba >= LOGICAL_PAGES || !mapped[lba] || l2p[lba].block != victim || l2p[lba].page != p)
                continue;
            if (driver.read_page({victim, p}, buffer.data()) != IOResult::OK) return false;
            if (append(lba, buffer.data(), false) != FTLResult::OK) return false;
            gc_stats.pages_copied++;
        }

        // 2. Apaga a vítima. Se ela morrer neste apagamento, os dados já
        //    estão a salvo em outro bloco: só a capacidade diminui.
        EraseResult erased = driver.erase_block(victim);
        note_erase(erased, victim);
        gc_stats.runs++;
        if (erased == EraseResult::OK) {
            is_free[victim] = true;
            free_count++;
        }
        return erased == EraseResult::OK || erased == EraseResult::WORN_OUT;
    }

protected:
    // Política de alocação: o bloco livre de MENOR NÚMERO (sem nivelamento).
    // É o ponto que a Etapa 6 vai sobrescrever com o wear leveling dinâmico.
    virtual uint32_t select_free_block() const {
        for (uint32_t b = 0; b < TOTAL_BLOCKS; ++b) {
            if (is_free[b]) return b;
        }
        return NO_BLOCK;
    }

    bool block_is_free(uint32_t block_id) const { return is_free[block_id]; }

public:
    explicit PageMappingFTL(IODriver& io) : FTL(io) { rebuild(); }

    const char* name() const override { return "Mapeamento de paginas sem wear leveling"; }

    FTLResult write(uint32_t lba, const uint8_t* data) override {
        if (lba >= LOGICAL_PAGES) return FTLResult::OUT_OF_RANGE;
        ftl_stats.host_writes++;
        return append(lba, data, true);
    }

    FTLResult read(uint32_t lba, uint8_t* buffer) override {
        if (lba >= LOGICAL_PAGES) return FTLResult::OUT_OF_RANGE;
        ftl_stats.host_reads++;
        if (!mapped[lba]) {
            std::memset(buffer, ERASED_BYTE, PAGE_SIZE);
            return FTLResult::OK;
        }
        return driver.read_page(l2p[lba], buffer) == IOResult::OK ? FTLResult::OK : FTLResult::IO_ERROR;
    }

    // Consultas usadas pelos testes e pela demonstração
    bool lookup(uint32_t lba, PhysicalAddress& pba) const {
        if (lba >= LOGICAL_PAGES || !mapped[lba]) return false;
        pba = l2p[lba];
        return true;
    }
    size_t free_blocks() const { return free_count; }
    uint32_t current_block() const { return active_block; }
    const GCStats& gc() const { return gc_stats; }
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
