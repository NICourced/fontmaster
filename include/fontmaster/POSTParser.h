#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace fontmaster {
namespace utils {

class POSTParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t postOffset;
    std::map<uint16_t, std::string> glyphNames;
    uint16_t numGlyphs;
    
public:
    POSTParser(const std::vector<uint8_t>& data, uint32_t offset, uint16_t glyphCount);
    bool parse();
    const std::map<uint16_t, std::string>& getGlyphNames() const;
};

}
}
