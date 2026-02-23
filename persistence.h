#pragma once
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Clock     = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;

/**
 * SnapshotEntry — one record in the snapshot file.
 */
struct SnapshotEntry {
    std::string key;
    std::string value;
    int64_t     ttl_ms = -1; // -1 = no expiry; >0 = remaining TTL in ms
};

/**
 * PersistenceEngine — binary snapshot save/load
 *
 * File Format (little-endian, sequential records):
 *   [4-byte magic "CSDB"]
 *   [4-byte version = 1]
 *   [8-byte record_count]
 *   Per record:
 *     [4-byte key_len][key bytes]
 *     [4-byte val_len][val bytes]
 *     [8-byte ttl_ms: -1 = no TTL]
 *
 * Note: we filter out records with ttl_ms == 0 at save time
 * (already expired keys are not written).
 */
class PersistenceEngine {
public:
    static constexpr uint32_t MAGIC   = 0x43534442; // 'CSDB'
    static constexpr uint32_t VERSION = 1;

    /**
     * Save entries to file.
     * @param filename  Output path.
     * @param entries   Vector of SnapshotEntry (caller filters expired).
     * @throws std::runtime_error on I/O failure.
     */
    static void save(const std::string& filename, const std::vector<SnapshotEntry>& entries) {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (!ofs) throw std::runtime_error("Cannot open file for writing: " + filename);

        // Header
        write32(ofs, MAGIC);
        write32(ofs, VERSION);
        write64(ofs, static_cast<int64_t>(entries.size()));

        for (auto& e : entries) {
            writeString(ofs, e.key);
            writeString(ofs, e.value);
            write64(ofs, e.ttl_ms);
        }
        ofs.flush();
        if (!ofs) throw std::runtime_error("Write error on file: " + filename);
    }

    /**
     * Load entries from file.
     * @param filename  Input path.
     * @return Vector of SnapshotEntry.
     * @throws std::runtime_error on I/O or format error.
     */
    static std::vector<SnapshotEntry> load(const std::string& filename) {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) throw std::runtime_error("Cannot open file for reading: " + filename);

        uint32_t magic   = read32(ifs);
        uint32_t version = read32(ifs);
        if (magic != MAGIC) throw std::runtime_error("Invalid snapshot file (bad magic)");
        if (version != VERSION) throw std::runtime_error("Unsupported snapshot version");

        int64_t count = read64(ifs);
        if (count < 0) throw std::runtime_error("Corrupt record count");

        std::vector<SnapshotEntry> entries;
        entries.reserve(static_cast<size_t>(count));

        for (int64_t i = 0; i < count; ++i) {
            SnapshotEntry e;
            e.key   = readString(ifs);
            e.value = readString(ifs);
            e.ttl_ms= read64(ifs);
            entries.push_back(std::move(e));
        }
        if (!ifs && !ifs.eof()) throw std::runtime_error("Read error on file: " + filename);
        return entries;
    }

private:
    static void write32(std::ofstream& ofs, uint32_t v) {
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
    static void write64(std::ofstream& ofs, int64_t v) {
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
    static void writeString(std::ofstream& ofs, const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        write32(ofs, len);
        ofs.write(s.data(), len);
    }

    static uint32_t read32(std::ifstream& ifs) {
        uint32_t v = 0;
        ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }
    static int64_t read64(std::ifstream& ifs) {
        int64_t v = 0;
        ifs.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }
    static std::string readString(std::ifstream& ifs) {
        uint32_t len = read32(ifs);
        if (len > 1024 * 1024) throw std::runtime_error("Implausible string length in snapshot");
        std::string s(len, '\0');
        ifs.read(s.data(), len);
        return s;
    }
};
