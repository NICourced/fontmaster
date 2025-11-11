#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace fontmaster {
namespace utils {

struct NameEntry {
    uint16_t platformID;
    uint16_t encodingID;
    uint16_t languageID;
    uint16_t nameID;
    std::vector<uint8_t> rawData;
    std::string getString() const;
};

class NAMEParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t nameOffset;
    std::map<uint16_t, std::string> glyphIDToName;
    std::vector<NameEntry> nameRecords;
    
public:
    NAMEParser(const std::vector<uint8_t>& data, uint32_t offset);
    bool parse();
    const std::map<uint16_t, std::string>& getGlyphNames() const;
    const std::vector<NameEntry>& getNameRecords() const;
    std::vector<NameEntry> getNameRecordsByID(uint16_t nameID) const;
    std::vector<NameEntry> getPostScriptNames() const;
};

}
}
