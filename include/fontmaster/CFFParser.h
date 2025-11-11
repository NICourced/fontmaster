#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace fontmaster {
namespace utils {

class CFFParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t cffOffset;
    std::map<uint16_t, std::string> glyphIDToName;
    
public:
    CFFParser(const std::vector<uint8_t>& data, uint32_t offset);
    bool parse();
    const std::map<uint16_t, std::string>& getGlyphNames() const;
};

}
}
