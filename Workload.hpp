#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "SimpleFS.hpp"

// ---------------------------------------------------------------------------
// Etapa 7: gerador de carga sobre o SimpleFS.
//
// Imita o uso de um pendrive:
//   1. Preparação: copia "fotos" e "musicas" (arquivos grandes) até ocupar
//      cerca de 60% do disco. Esses arquivos nunca mudam (dados FRIOS).
//   2. Uso: repete operações típicas de quem edita trabalhos no pendrive:
//        70% editar um documento pequeno (regrava 2 a 16 KB);
//        20% criar um documento novo (se o diretório estiver cheio, apaga um);
//        10% apagar um documento.
//      Toda operação regrava superbloco, diretório e FAT (dados QUENTES).
//
// A semente é fixa, então a mesma sequência de operações é aplicada a todos
// os modos da FTL: a comparação é justa.
// ---------------------------------------------------------------------------

struct WorkloadStats {
    uint64_t steps = 0;    // operações executadas na fase de uso
    uint64_t edits = 0;
    uint64_t creates = 0;
    uint64_t removes = 0;
    uint32_t cold_files = 0;
    uint64_t cold_bytes = 0;
};

class FSWorkload {
private:
    SimpleFS& fs;
    std::mt19937 rng;
    std::vector<std::string> docs;   // documentos (arquivos que mudam)
    uint32_t next_doc = 0;
    uint32_t version = 0;
    WorkloadStats stats_;

    // Conteúdo determinístico: depende só do nome e da versão
    static std::vector<uint8_t> content(const std::string& name, uint32_t ver, size_t size) {
        std::vector<uint8_t> data(size);
        uint32_t seed = ver * 2654435761u;
        for (char c : name) seed = seed * 31 + static_cast<uint8_t>(c);
        for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(seed + i * 7);
        return data;
    }

    size_t random_size(size_t min_kb, size_t max_kb) {
        std::uniform_int_distribution<size_t> kb(min_kb, max_kb);
        return kb(rng) * 1024;
    }

    FSResult new_document() {
        std::string name = "trabalho_" + std::to_string(next_doc++) + ".txt";
        FSResult r = fs.write_file(name, content(name, ++version, random_size(2, 16)));
        if (r == FSResult::OK) docs.push_back(name);
        return r;
    }

public:
    // Arquivos frios ocupam até esta fração do disco na preparação
    static constexpr double COLD_FRACTION = 0.6;

    // Documentos que já existem no disco (de uma sessão anterior) continuam
    // sendo editados, e os nomes novos não repetem os antigos.
    FSWorkload(SimpleFS& file_system, uint32_t seed = 2026) : fs(file_system), rng(seed) {
        for (const FileInfo& f : fs.list()) {
            if (f.name.rfind("trabalho_", 0) != 0) continue;
            docs.push_back(f.name);
            uint32_t n = static_cast<uint32_t>(std::strtoul(f.name.c_str() + 9, nullptr, 10));
            next_doc = std::max(next_doc, n + 1);
        }
    }

    // A preparação só faz sentido com o disco quase vazio
    bool needs_preparation() const { return fs.free_pages() > FS_DATA_LBAS / 2; }

    // Fase 1: arquivos grandes que não mudam mais (dados frios)
    FSResult prepare() {
        const uint64_t target = static_cast<uint64_t>(COLD_FRACTION * FS_DATA_LBAS * PAGE_SIZE);
        uint32_t n = 0;
        while (stats_.cold_bytes < target && fs.file_count() + 8 < FS_MAX_FILES) {
            std::string name = (n % 2 == 0 ? "foto_" : "musica_") + std::to_string(n) + (n % 2 == 0 ? ".jpg" : ".mp3");
            size_t size = random_size(256, 900);
            FSResult r = fs.write_file(name, content(name, 0, size));
            if (r != FSResult::OK) return r;
            stats_.cold_bytes += size;
            stats_.cold_files++;
            n++;
        }
        // Alguns documentos iniciais
        for (int i = 0; i < 4; ++i) {
            FSResult r = new_document();
            if (r != FSResult::OK) return r;
        }
        return FSResult::OK;
    }

    // Fase 2: uma operação de uso
    FSResult step() {
        std::uniform_int_distribution<int> percent(0, 99);
        int p = percent(rng);
        FSResult r;
        stats_.steps++;
        if (p < 70 && !docs.empty()) {
            std::uniform_int_distribution<size_t> pick(0, docs.size() - 1);
            const std::string& name = docs[pick(rng)];
            r = fs.write_file(name, content(name, ++version, random_size(2, 16)));
            stats_.edits++;
        } else if ((p < 90 && fs.file_count() < FS_MAX_FILES) || docs.size() <= 2) {
            // Diretório cheio: em vez de criar, o usuário apaga um documento (abaixo)
            r = new_document();
            stats_.creates++;
        } else {
            std::uniform_int_distribution<size_t> pick(0, docs.size() - 1);
            size_t i = pick(rng);
            r = fs.remove(docs[i]);
            docs.erase(docs.begin() + static_cast<long>(i));
            stats_.removes++;
        }
        return r;
    }

    const WorkloadStats& stats() const { return stats_; }
    const std::vector<std::string>& documents() const { return docs; }
};
