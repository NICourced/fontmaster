#include "fontmaster/MAXPParser.h"  // Changed from TTFUtils.h to the specific header
#include <vector>
#include <stdexcept>
#include <iostream>

namespace fontmaster {
namespace utils {

// Implement the public class methods
MAXPParser::MAXPParser(const std::vector<uint8_t>& data, uint32_t offset)
    : fontData(data), maxpOffset(offset), numGlyphs(0) {}

bool MAXPParser::parse() {
    try {
        if (maxpOffset + 6 > fontData.size()) {
            return false;
        }

        const uint8_t* data = fontData.data() + maxpOffset;

        uint32_t version = readUInt32(data);  // Potentially unused variable
        (void)version;  // Suppress unused variable warning
        numGlyphs = readUInt16(data + 4);

        // Note: Some fields are not defined in the header, so we ignore them
        // to match the header declaration

        return true;
    } catch (const std::exception& e) {
        std::cerr << "MAXP table parsing error: " << e.what() << std::endl;
        return false;
    }
}

uint16_t MAXPParser::getNumGlyphs() const {
    return numGlyphs;
}

// Helper methods (not declared in header, so they're private)
uint16_t MAXPParser::readUInt16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

uint32_t MAXPParser::readUInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

} // namespace utils
} // namespace fontmaster
