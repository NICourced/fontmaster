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
    
    const TTFHeader* header = reinterpret_cast<const TTFHeader*>(fontData.data());
    size_t expectedSize = sizeof(TTFHeader) + header->numTables * sizeof(TableRecord);
    
    if (fontData.size() < expectedSize) {
        throw std::runtime_error("Font data too small for table records");
    }
    
    const TableRecord* tableRecords = reinterpret_cast<const TableRecord*>(
        fontData.data() + sizeof(TTFHeader));
    
    for (uint16_t i = 0; i < header->numTables; ++i) {
        tables.push_back(tableRecords[i]);
    }
    
    return tables;
}

bool hasTable(const std::vector<TableRecord>& tables, const std::string& tableTag) {
    return std::any_of(tables.begin(), tables.end(), 
                      [&](const TableRecord& table) { 
                          return memcmp(table.tag, tableTag.c_str(), 4) == 0; 
                      });
}

const TableRecord* findTable(const std::vector<TableRecord>& tables, const std::string& tableTag) {
    for (const auto& table : tables) {
        if (memcmp(table.tag, tableTag.c_str(), 4) == 0) {
            return &table;
        }
    }
    return nullptr;
}

} // namespace utils
} // namespace fontmaster
