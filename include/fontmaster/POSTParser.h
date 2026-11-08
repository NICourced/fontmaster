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

private:
    uint16_t readUInt16(const uint8_t* data);
    uint32_t readUInt32(const uint8_t* data);
    int16_t readInt16(const uint8_t* data);
    int8_t readInt8(const uint8_t* data);
    bool parseVersion1(const uint8_t* data);
    bool parseVersion2(const uint8_t* data);
    bool parseVersion25(const uint8_t* data);
    bool parseVersion3(const uint8_t* data);
};

}
}
