#include "fontmaster/TTFUtils.h"
#include <algorithm>
#include <fstream>

namespace fontmaster {
namespace utils {

std::vector<TableRecord> parseTTFTables(const std::vector<uint8_t>& fontData) {
    std::vector<TableRecord> tables;

    if (fontData.size() < sizeof(TTFHeader)) {
        throw std::runtime_error("Font data too small for TTF header");
    }

    // Read header fields manually to avoid potential alignment issues
    if (fontData.size() < 6) {  // Need at least sfntVersion (4) + numTables (2)
        throw std::runtime_error("Font data too small for TTF header");
    }

    const uint32_t version = (uint32_t(fontData[0]) << 24) | (uint32_t(fontData[1]) << 16) |
                             (uint32_t(fontData[2]) << 8) | fontData[3];
    if (version != 0x00010000 && version != 0x4F54544F && version != 0x74727565)
        throw std::runtime_error("Unsupported sfnt signature (expected TrueType or OpenType)");
    uint16_t numTables = (fontData[4] << 8) | fontData[5];

    size_t expectedSize = 12 + numTables * 16;
    if (fontData.size() < expectedSize) {
        throw std::runtime_error("Font data too small for table records. Need " +
                                std::to_string(expectedSize) + " bytes, have " +
                                std::to_string(fontData.size()));
    }

    // Read table records manually to avoid potential alignment issues
    for (uint16_t i = 0; i < numTables; ++i) {
        size_t offset = 12 + i * 16;
        if (offset + 16 > fontData.size()) {
            throw std::runtime_error("Font data too small for table record " + std::to_string(i));
        }

        TableRecord record;
        // Copy tag (4 bytes)
        record.tag[0] = fontData[offset];
        record.tag[1] = fontData[offset + 1];
        record.tag[2] = fontData[offset + 2];
        record.tag[3] = fontData[offset + 3];

        // Read checksum (4 bytes)
        record.checksum = (fontData[offset + 4] << 24) | (fontData[offset + 5] << 16) |
                          (fontData[offset + 6] << 8) | fontData[offset + 7];

        // Read offset (4 bytes)
        record.offset = (fontData[offset + 8] << 24) | (fontData[offset + 9] << 16) |
                        (fontData[offset + 10] << 8) | fontData[offset + 11];

        // Read length (4 bytes)
        record.length = (fontData[offset + 12] << 24) | (fontData[offset + 13] << 16) |
                        (fontData[offset + 14] << 8) | fontData[offset + 15];

        if (record.offset > fontData.size() || record.length > fontData.size() - record.offset)
            throw std::runtime_error("Font table outside file bounds: " + std::string(record.tag, 4));
        tables.push_back(record);
    }

    return tables;
}

bool hasTable(const std::vector<TableRecord>& tables, const std::string& tableTag) {
    if (tableTag.size() != 4) return false;
    return std::any_of(tables.begin(), tables.end(),
                      [&](const TableRecord& table) {
                          return memcmp(table.tag, tableTag.c_str(), 4) == 0;
                      });
}

const TableRecord* findTable(const std::vector<TableRecord>& tables, const std::string& tableTag) {
    if (tableTag.size() != 4) return nullptr;
    for (const auto& table : tables) {
        if (memcmp(table.tag, tableTag.c_str(), 4) == 0) {
            return &table;
        }
    }
    return nullptr;
}

std::vector<uint8_t> readFontFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open font file: " + filepath);
    const auto size = file.tellg();
    if (size < 0) throw std::runtime_error("Cannot determine font file size");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read font file: " + filepath);
    return bytes;
}

std::vector<uint8_t> rebuildSfnt(const std::vector<uint8_t>& original,
    const std::map<std::string, std::vector<uint8_t>>& replacements,
    const std::set<std::string>& removedTables) {
    const auto records = parseTTFTables(original);
    std::map<std::string, std::vector<uint8_t>> tables;
    for (const auto& record : records) {
        const std::string tag(record.tag, 4);
        if (removedTables.count(tag)) continue;
        if (!tables.emplace(tag, std::vector<uint8_t>(original.begin() + record.offset,
                            original.begin() + record.offset + record.length)).second)
            throw std::runtime_error("Duplicate sfnt table: " + tag);
    }
    for (const auto& replacement : replacements) {
        if (replacement.first.size() != 4) throw std::runtime_error("Invalid sfnt table tag");
        if (!removedTables.count(replacement.first)) tables[replacement.first] = replacement.second;
    }
    if (tables.empty() || tables.size() > 4095) throw std::runtime_error("Invalid sfnt table count");
    std::vector<uint8_t> result(original.begin(), original.begin() + 12);
    result.resize(12 + tables.size() * 16, 0);
    auto put16 = [](std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
        bytes.at(offset) = value >> 8; bytes.at(offset + 1) = value;
    };
    auto put32 = [](std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes.at(offset + i) = value >> (24 - i * 8);
    };
    auto checksum = [](const std::vector<uint8_t>& bytes) {
        uint32_t sum = 0;
        for (size_t i = 0; i < bytes.size(); ++i)
            sum += uint32_t(bytes[i]) << (24 - (i % 4) * 8);
        return sum;
    };
    uint16_t power = 1, selector = 0;
    while (size_t(power) * 2 <= tables.size()) { power *= 2; ++selector; }
    put16(result, 4, tables.size());
    put16(result, 6, power * 16);
    put16(result, 8, selector);
    put16(result, 10, (tables.size() - power) * 16);
    size_t headOffset = 0, index = 0;
    for (auto& entry : tables) {
        const auto& tag = entry.first;
        auto& bytes = entry.second;
        if (tag == "head") {
            if (bytes.size() < 12) throw std::runtime_error("Truncated head table");
            put32(bytes, 8, 0);
            headOffset = result.size();
        }
        if (bytes.size() > UINT32_MAX || result.size() > UINT32_MAX - bytes.size() - 3)
            throw std::runtime_error("sfnt data exceeds 32-bit offsets");
        const size_t record = 12 + index++ * 16;
        std::copy(tag.begin(), tag.end(), result.begin() + record);
        put32(result, record + 4, checksum(bytes));
        put32(result, record + 8, result.size());
        put32(result, record + 12, bytes.size());
        result.insert(result.end(), bytes.begin(), bytes.end());
        while (result.size() % 4) result.push_back(0);
    }
    if (headOffset) put32(result, headOffset + 8, 0xB1B0AFBAu - checksum(result));
    return result;
}

} // namespace utils
} // namespace fontmaster
