#include "fontmaster/CFFParser.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {
namespace utils {

CFFParser::CFFParser(const std::vector<uint8_t>& data, uint32_t offset) 
    : fontData(data), baseOffset(offset) {}

bool CFFParser::parse() {
    if (fontData.size() < baseOffset + 4) {
        std::cerr << "CFF: Font data too small" << std::endl;
        return false;
    }

    size_t offset = baseOffset;
    const uint8_t* current = fontData.data() + baseOffset;

    // Parse CFF header
    uint8_t major = current[0];
    (void)current[1]; // minor
    uint8_t hdrSize = current[2];
    (void)current[3]; // offSize

    if (major != 1) {
        std::cerr << "CFF: Unsupported major version: " << static_cast<int>(major) << std::endl;
        return false;
    }

    offset = baseOffset + hdrSize;

    // Parse Name INDEX
    if (!parseIndex(offset)) {
        std::cerr << "CFF: Failed to parse Name INDEX" << std::endl;
        return false;
    }

    // Parse Top DICT INDEX
    if (!parseIndex(offset)) {
        std::cerr << "CFF: Failed to parse Top DICT INDEX" << std::endl;
        return false;
    }

    // Parse String INDEX
    if (!parseIndex(offset)) {
        std::cerr << "CFF: Failed to parse String INDEX" << std::endl;
        return false;
    }

    // Parse Global Subr INDEX
    if (!parseIndex(offset)) {
        std::cerr << "CFF: Failed to parse Global Subr INDEX" << std::endl;
        return false;
    }

    std::cout << "CFF: Successfully parsed CFF data" << std::endl;
    return true;
}

bool CFFParser::parseIndex(size_t& offset) {
    if (offset + 2 > fontData.size()) {
        return false;
    }

    const uint8_t* data = fontData.data() + offset;
    uint16_t count = (data[0] << 8) | data[1];

    if (count == 0) {
        offset += 2;
        return true;
    }

    if (offset + 2 + 1 > fontData.size()) {
        return false;
    }

    uint8_t offSize = data[2];
    // Убраны неиспользуемые переменные
    // size_t objectDataStart = indexStart + (count + 1) * offSize;

    // Calculate total index size
    size_t indexSize = 2 + 1 + (count + 1) * offSize;

    // Read offsets to calculate object data size
    (void)readOffset(data + 3, offSize); // firstOffset
    uint32_t lastOffset = readOffset(data + 3 + count * offSize, offSize);
    size_t objectDataSize = lastOffset - 1;

    indexSize += objectDataSize;

    if (offset + indexSize > fontData.size()) {
        return false;
    }

    offset += indexSize;
    return true;
}

uint32_t CFFParser::readOffset(const uint8_t* data, uint8_t offSize) {
    uint32_t offset = 0;
    for (uint8_t i = 0; i < offSize; ++i) {
        offset = (offset << 8) | data[i];
    }
    return offset;
}

} // namespace utils
} // namespace fontmaster
