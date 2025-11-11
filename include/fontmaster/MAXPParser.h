#pragma once
#include <vector>
#include <cstdint>

namespace fontmaster {
namespace utils {

class MAXPParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t maxpOffset;
    uint16_t numGlyphs;
    
public:
    MAXPParser(const std::vector<uint8_t>& data, uint32_t offset);
    bool parse();
    uint16_t getNumGlyphs() const;
};

}
}
