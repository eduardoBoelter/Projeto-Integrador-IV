#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "FTL.hpp"

// ---------------------------------------------------------------------------
// Etapa 7: sistema de arquivos mínimo (SimpleFS).
//
// Não é um sistema de arquivos completo: existe para GERAR A CARGA DE ESCRITA
// que um sistema de arquivos real gera sobre um pendrive. Como o FAT, ele
// guarda os metadados em LBAs fixos, que são regravados a cada operação
// (dados quentes), enquanto o conteúdo dos arquivos quase não muda (dados frios).
//
// Organização dos LBAs (páginas de 2 KB vistas pela FTL):
//
//   LBA 0        superbloco: assinatura, contadores e espaço livre
//   LBAs 1 a 8   tabela de alocação (FAT): 1 entrada de 2 bytes por LBA de dados
//   LBA 9        diretório: até 32 arquivos de 64 bytes cada
//   LBAs 10 ...  área de dados (7.670 LBAs, cerca de 15 MB)
//
// Toda operação que muda arquivos regrava o superbloco, o diretório e as
// páginas da FAT que mudaram. É esse padrão que concentra o desgaste.
// ---------------------------------------------------------------------------

constexpr uint32_t FS_SUPERBLOCK_LBA = 0;
constexpr uint32_t FS_FAT_FIRST_LBA = 1;
constexpr uint32_t FS_FAT_PAGES = 8;
constexpr uint32_t FS_DIR_LBA = FS_FAT_FIRST_LBA + FS_FAT_PAGES;   // 9
constexpr uint32_t FS_DATA_FIRST_LBA = FS_DIR_LBA + 1;             // 10
constexpr uint32_t FS_DATA_LBAS = LOGICAL_PAGES - FS_DATA_FIRST_LBA; // 7670
constexpr uint32_t FS_MAX_FILES = 32;
constexpr size_t FS_NAME_MAX = 39;                                 // + '\0' = 40 bytes
constexpr size_t FS_DIR_ENTRY_SIZE = 64;

// Valores especiais da FAT
constexpr uint16_t FAT_FREE = 0xFFFF;  // LBA de dados livre
constexpr uint16_t FAT_END = 0xFFFE;   // último LBA de um arquivo

constexpr char FS_MAGIC[8] = {'S', 'I', 'M', 'P', 'L', 'E', 'F', 'S'};

static_assert(FS_DATA_LBAS * 2 <= FS_FAT_PAGES * PAGE_SIZE, "a FAT precisa caber nas suas páginas");
static_assert(FS_MAX_FILES * FS_DIR_ENTRY_SIZE <= PAGE_SIZE, "o diretório precisa caber em uma página");

enum class FSResult : uint8_t {
    OK = 0,
    NOT_FORMATTED = 1,  // não há SimpleFS no dispositivo
    NOT_FOUND = 2,      // arquivo não existe
    ALREADY_EXISTS = 3,
    NAME_INVALID = 4,   // nome vazio ou longo demais
    DIRECTORY_FULL = 5,
    NO_SPACE = 6,       // não há LBAs de dados livres suficientes
    DEVICE_ERROR = 7    // a FTL recusou a operação (por exemplo, dispositivo gasto)
};

inline const char* to_string(FSResult result) {
    switch (result) {
        case FSResult::OK:             return "OK";
        case FSResult::NOT_FORMATTED:  return "NOT_FORMATTED";
        case FSResult::NOT_FOUND:      return "NOT_FOUND";
        case FSResult::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case FSResult::NAME_INVALID:   return "NAME_INVALID";
        case FSResult::DIRECTORY_FULL: return "DIRECTORY_FULL";
        case FSResult::NO_SPACE:       return "NO_SPACE";
        case FSResult::DEVICE_ERROR:   return "DEVICE_ERROR";
    }
    return "?";
}

// Um arquivo, como aparece no diretório
struct FileInfo {
    std::string name;
    uint32_t size = 0;        // em bytes
    uint32_t pages = 0;       // LBAs de dados ocupados
    uint16_t first = FAT_END; // primeiro LBA de dados (índice relativo à área de dados)
};

// Contadores do ponto de vista do sistema de arquivos
struct FSStats {
    uint64_t operations = 0;      // operações que alteraram o disco
    uint64_t metadata_writes = 0; // páginas de superbloco, FAT e diretório gravadas
    uint64_t data_writes = 0;     // páginas de conteúdo de arquivos gravadas
};

class SimpleFS {
private:
    FTL& ftl;
    bool mounted = false;
    std::vector<uint16_t> fat;            // próxima entrada da cadeia de cada LBA de dados
    std::vector<FileInfo> files;          // diretório em memória (posição = entrada)
    std::vector<bool> used;               // a entrada do diretório está ocupada?
    std::vector<bool> fat_dirty;          // páginas da FAT alteradas desde a última gravação
    uint32_t free_lbas = 0;
    uint32_t operation_counter = 0;       // gravado no superbloco a cada operação
    FSStats fs_stats;

    // --- Gravação dos metadados (dados quentes) ---

    bool write_lba(uint32_t lba, const std::vector<uint8_t>& page, bool metadata) {
        if (ftl.write(lba, page.data()) != FTLResult::OK) return false;
        (metadata ? fs_stats.metadata_writes : fs_stats.data_writes)++;
        return true;
    }

    bool write_superblock() {
        std::vector<uint8_t> page(PAGE_SIZE, 0);
        std::memcpy(page.data(), FS_MAGIC, sizeof(FS_MAGIC));
        uint32_t fields[5] = {1 /* versão */, FS_DATA_LBAS, free_lbas, file_count(), operation_counter};
        std::memcpy(page.data() + 8, fields, sizeof(fields));
        return write_lba(FS_SUPERBLOCK_LBA, page, true);
    }

    bool write_fat_page(uint32_t index) {
        std::vector<uint8_t> page(PAGE_SIZE, 0xFF);
        const size_t per_page = PAGE_SIZE / 2;
        size_t first = index * per_page;
        size_t count = std::min(per_page, fat.size() - std::min(first, fat.size()));
        if (count > 0) std::memcpy(page.data(), &fat[first], count * 2);
        return write_lba(FS_FAT_FIRST_LBA + index, page, true);
    }

    bool write_directory() {
        std::vector<uint8_t> page(PAGE_SIZE, 0);
        for (uint32_t i = 0; i < FS_MAX_FILES; ++i) {
            if (!used[i]) continue;
            uint8_t* entry = page.data() + i * FS_DIR_ENTRY_SIZE;
            entry[0] = 1; // ocupada
            std::memcpy(entry + 1, files[i].name.c_str(), files[i].name.size());
            std::memcpy(entry + 41, &files[i].size, 4);
            std::memcpy(entry + 45, &files[i].pages, 4);
            std::memcpy(entry + 49, &files[i].first, 2);
        }
        return write_lba(FS_DIR_LBA, page, true);
    }

    // Grava os metadados alterados por uma operação: FAT (só as páginas que
    // mudaram), diretório e superbloco. É aqui que nascem os dados quentes.
    bool flush_metadata() {
        operation_counter++;
        fs_stats.operations++;
        for (uint32_t i = 0; i < FS_FAT_PAGES; ++i) {
            if (!fat_dirty[i]) continue;
            if (!write_fat_page(i)) return false;
            fat_dirty[i] = false;
        }
        return write_directory() && write_superblock();
    }

    void set_fat(uint32_t index, uint16_t value) {
        fat[index] = value;
        fat_dirty[index / (PAGE_SIZE / 2)] = true;
    }

    int find(const std::string& name) const {
        for (uint32_t i = 0; i < FS_MAX_FILES; ++i)
            if (used[i] && files[i].name == name) return static_cast<int>(i);
        return -1;
    }

    // Libera a cadeia de LBAs de um arquivo na FAT
    void free_chain(uint16_t first) {
        uint16_t current = first;
        while (current != FAT_END && current < FS_DATA_LBAS) {
            uint16_t next = fat[current];
            set_fat(current, FAT_FREE);
            free_lbas++;
            current = next;
        }
    }

    // Reserva `count` LBAs de dados livres (o primeiro livre, como no FAT)
    std::vector<uint16_t> allocate(uint32_t count) {
        std::vector<uint16_t> chain;
        for (uint32_t i = 0; i < FS_DATA_LBAS && chain.size() < count; ++i)
            if (fat[i] == FAT_FREE) chain.push_back(static_cast<uint16_t>(i));
        return chain;
    }

    static bool valid_name(const std::string& name) {
        return !name.empty() && name.size() <= FS_NAME_MAX && name.find(' ') == std::string::npos;
    }

public:
    explicit SimpleFS(FTL& translation_layer) : ftl(translation_layer) {}

    // Cria um sistema de arquivos vazio (grava superbloco, FAT e diretório)
    FSResult format() {
        fat.assign(FS_DATA_LBAS, FAT_FREE);
        files.assign(FS_MAX_FILES, FileInfo{});
        used.assign(FS_MAX_FILES, false);
        fat_dirty.assign(FS_FAT_PAGES, true);
        free_lbas = FS_DATA_LBAS;
        operation_counter = 0;
        mounted = true;
        return flush_metadata() ? FSResult::OK : FSResult::DEVICE_ERROR;
    }

    // Lê superbloco, FAT e diretório de um dispositivo já formatado
    FSResult mount() {
        std::vector<uint8_t> page(PAGE_SIZE);
        if (ftl.read(FS_SUPERBLOCK_LBA, page.data()) != FTLResult::OK) return FSResult::DEVICE_ERROR;
        if (std::memcmp(page.data(), FS_MAGIC, sizeof(FS_MAGIC)) != 0) return FSResult::NOT_FORMATTED;
        uint32_t fields[5];
        std::memcpy(fields, page.data() + 8, sizeof(fields));
        if (fields[1] != FS_DATA_LBAS) return FSResult::NOT_FORMATTED;
        operation_counter = fields[4];

        fat.assign(FS_DATA_LBAS, FAT_FREE);
        const size_t per_page = PAGE_SIZE / 2;
        for (uint32_t i = 0; i < FS_FAT_PAGES; ++i) {
            if (ftl.read(FS_FAT_FIRST_LBA + i, page.data()) != FTLResult::OK) return FSResult::DEVICE_ERROR;
            size_t first = i * per_page;
            if (first >= fat.size()) break;
            size_t count = std::min(per_page, fat.size() - first);
            std::memcpy(&fat[first], page.data(), count * 2);
        }
        free_lbas = static_cast<uint32_t>(std::count(fat.begin(), fat.end(), FAT_FREE));

        files.assign(FS_MAX_FILES, FileInfo{});
        used.assign(FS_MAX_FILES, false);
        if (ftl.read(FS_DIR_LBA, page.data()) != FTLResult::OK) return FSResult::DEVICE_ERROR;
        for (uint32_t i = 0; i < FS_MAX_FILES; ++i) {
            const uint8_t* entry = page.data() + i * FS_DIR_ENTRY_SIZE;
            if (entry[0] != 1) continue;
            used[i] = true;
            char name[FS_NAME_MAX + 1] = {0};
            std::memcpy(name, entry + 1, FS_NAME_MAX);
            files[i].name = name;
            std::memcpy(&files[i].size, entry + 41, 4);
            std::memcpy(&files[i].pages, entry + 45, 4);
            std::memcpy(&files[i].first, entry + 49, 2);
        }
        fat_dirty.assign(FS_FAT_PAGES, false);
        mounted = true;
        return FSResult::OK;
    }

    // Cria um arquivo vazio
    FSResult create(const std::string& name) {
        if (!mounted) return FSResult::NOT_FORMATTED;
        if (!valid_name(name)) return FSResult::NAME_INVALID;
        if (find(name) >= 0) return FSResult::ALREADY_EXISTS;
        for (uint32_t i = 0; i < FS_MAX_FILES; ++i) {
            if (used[i]) continue;
            used[i] = true;
            files[i] = FileInfo{name, 0, 0, FAT_END};
            return flush_metadata() ? FSResult::OK : FSResult::DEVICE_ERROR;
        }
        return FSResult::DIRECTORY_FULL;
    }

    // Substitui o conteúdo do arquivo (cria se não existir). Como num FAT real,
    // os LBAs antigos são liberados e o conteúdo novo vai para os primeiros
    // LBAs livres; depois a FAT, o diretório e o superbloco são regravados.
    FSResult write_file(const std::string& name, const std::vector<uint8_t>& content) {
        if (!mounted) return FSResult::NOT_FORMATTED;
        int index = find(name);
        if (index < 0) {
            FSResult created = create(name);
            if (created != FSResult::OK) return created;
            index = find(name);
        }
        FileInfo& file = files[index];
        uint32_t needed = static_cast<uint32_t>((content.size() + PAGE_SIZE - 1) / PAGE_SIZE);
        if (needed > free_lbas + file.pages) return FSResult::NO_SPACE;

        free_chain(file.first);
        std::vector<uint16_t> chain = allocate(needed);
        for (size_t i = 0; i < chain.size(); ++i) {
            set_fat(chain[i], i + 1 < chain.size() ? chain[i + 1] : FAT_END);
            free_lbas--;
            std::vector<uint8_t> page(PAGE_SIZE, ERASED_BYTE);
            size_t offset = i * PAGE_SIZE;
            std::memcpy(page.data(), content.data() + offset, std::min(PAGE_SIZE, content.size() - offset));
            if (!write_lba(FS_DATA_FIRST_LBA + chain[i], page, false)) return FSResult::DEVICE_ERROR;
        }
        file.size = static_cast<uint32_t>(content.size());
        file.pages = needed;
        file.first = chain.empty() ? FAT_END : chain.front();
        return flush_metadata() ? FSResult::OK : FSResult::DEVICE_ERROR;
    }

    FSResult read_file(const std::string& name, std::vector<uint8_t>& content) {
        if (!mounted) return FSResult::NOT_FORMATTED;
        int index = find(name);
        if (index < 0) return FSResult::NOT_FOUND;
        const FileInfo& file = files[index];
        content.assign(file.size, 0);
        std::vector<uint8_t> page(PAGE_SIZE);
        uint16_t current = file.first;
        for (uint32_t i = 0; i < file.pages && current < FS_DATA_LBAS; ++i) {
            if (ftl.read(FS_DATA_FIRST_LBA + current, page.data()) != FTLResult::OK) return FSResult::DEVICE_ERROR;
            size_t offset = static_cast<size_t>(i) * PAGE_SIZE;
            std::memcpy(content.data() + offset, page.data(), std::min(PAGE_SIZE, content.size() - offset));
            current = fat[current];
        }
        return FSResult::OK;
    }

    FSResult remove(const std::string& name) {
        if (!mounted) return FSResult::NOT_FORMATTED;
        int index = find(name);
        if (index < 0) return FSResult::NOT_FOUND;
        free_chain(files[index].first);
        used[index] = false;
        files[index] = FileInfo{};
        return flush_metadata() ? FSResult::OK : FSResult::DEVICE_ERROR;
    }

    std::vector<FileInfo> list() const {
        std::vector<FileInfo> result;
        for (uint32_t i = 0; i < FS_MAX_FILES; ++i)
            if (used[i]) result.push_back(files[i]);
        return result;
    }

    bool exists(const std::string& name) const { return find(name) >= 0; }
    bool is_mounted() const { return mounted; }
    uint32_t file_count() const { return static_cast<uint32_t>(std::count(used.begin(), used.end(), true)); }
    uint32_t free_pages() const { return free_lbas; }
    uint64_t free_bytes() const { return static_cast<uint64_t>(free_lbas) * PAGE_SIZE; }
    const FSStats& stats() const { return fs_stats; }
    FTL& translation_layer() { return ftl; }
};
