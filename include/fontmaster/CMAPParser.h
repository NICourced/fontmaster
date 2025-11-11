#pragma once
#include <vector>
#include <map>
#include <cstdint>

namespace fontmaster {
namespace utils {

class CMAPParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t cmapOffset;
    std::map<uint32_t, uint16_t> unicodeToGlyphID;
    std::map<uint16_t, uint32_t> glyphIDToUnicode;
    
public:
    CMAPParser(const std::vector<uint8_t>& data, uint32_t offset);
    bool parse();
    const std::map<uint32_t, uint16_t>& getUnicodeToGlyphMap() const;
    const std::map<uint16_t, uint32_t>& getGlyphToUnicodeMap() const;
};

}
}
