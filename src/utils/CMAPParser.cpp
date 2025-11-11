#include "fontmaster/CMAPParser.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {
namespace utils {

CMAPParser::CMAPParser(const std::vector<uint8_t>& fontData) : fontData(fontData) {}

bool CMAPParser::parse() {
    if (fontData.size() < 4) {
        std::cerr << "CMAP: Font data too small" << std::endl;
        return false;
    }

    uint16_t version = readUInt16(fontData.data());
    uint16_t numTables = readUInt16(fontData.data() + 2);

    if (version != 0) {
        std::cerr << "CMAP: Unsupported version: " << version << std::endl;
        return false;
    }

    std::cout << "CMAP: Parsing " << numTables << " encoding tables" << std::endl;

    for (uint16_t i = 0; i < numTables; ++i) {
        uint32_t tableOffset = 4 + i * 8;
        if (tableOffset + 8 > fontData.size()) {
            std::cerr << "CMAP: Table record " << i << " extends beyond font data" << std::endl;
            continue;
        }

        uint16_t platformID = readUInt16(fontData.data() + tableOffset);
        uint16_t encodingID = readUInt16(fontData.data() + tableOffset + 2);
        uint32_t subtableOffset = readUInt32(fontData.data() + tableOffset + 4);

        if (subtableOffset >= fontData.size()) {
            std::cerr << "CMAP: Subtable offset " << subtableOffset << " beyond font data" << std::endl;
            continue;
        }

        parseSubtable(subtableOffset, platformID, encodingID);
    }

    return true;
}

uint16_t CMAPParser::getGlyphIndex(uint32_t charCode) const {
    auto it = charToGlyph.find(charCode);
    return (it != charToGlyph.end()) ? it->second : 0;
}

std::set<uint32_t> CMAPParser::getCharCodes(uint16_t glyphIndex) const {
    auto it = glyphToChar.find(glyphIndex);
    return (it != glyphToChar.end()) ? it->second : std::set<uint32_t>();
}

void CMAPParser::parseSubtable(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 2 > fontData.size()) {
        return;
    }

    uint16_t format = readUInt16(fontData.data() + offset);

    switch (format) {
        case 0: parseFormat0(offset, platformID, encodingID); break;
        case 2: parseFormat2(offset, platformID, encodingID); break;
        case 4: parseFormat4(offset, platformID, encodingID); break;
        case 6: parseFormat6(offset, platformID, encodingID); break;
        case 8: parseFormat8(offset, platformID, encodingID); break;
        case 10: parseFormat10(offset, platformID, encodingID); break;
        case 12: parseFormat12(offset, platformID, encodingID); break;
        case 13: parseFormat13(offset, platformID, encodingID); break;
        case 14: parseFormat14(offset, platformID, encodingID); break;
        default:
            std::cout << "CMAP: Unsupported format: " << format << std::endl;
            break;
    }
}

void CMAPParser::parseFormat0(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 6 + 256 > fontData.size()) {
        std::cerr << "CMAP: Format 0 table too small" << std::endl;
        return;
    }

    // Read but don't use these variables to avoid warnings
    (void)readUInt16(fontData.data() + offset + 2); // length
    (void)readUInt16(fontData.data() + offset + 4); // language
    (void)platformID;
    (void)encodingID;

    for (int i = 0; i < 256; ++i) {
        uint8_t glyphIndex = fontData[offset + 6 + i];
        if (glyphIndex != 0) {
            charToGlyph[static_cast<uint32_t>(i)] = glyphIndex;
            glyphToChar[glyphIndex].insert(static_cast<uint32_t>(i));
        }
    }

    std::cout << "CMAP: Format 0 parsed, " << charToGlyph.size() << " mappings" << std::endl;
}

void CMAPParser::parseFormat4(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 14 > fontData.size()) {
        std::cerr << "CMAP: Format 4 table too small" << std::endl;
        return;
    }

    // Read but don't use these variables to avoid warnings
    (void)readUInt16(fontData.data() + offset + 2); // length
    (void)readUInt16(fontData.data() + offset + 4); // language
    (void)platformID;
    (void)encodingID;

    uint16_t segCountX2 = readUInt16(fontData.data() + offset + 6);
    uint16_t segCount = segCountX2 / 2;

    uint32_t endCountOffset = offset + 14;
    uint32_t startCountOffset = endCountOffset + segCountX2 + 2;
    uint32_t idDeltaOffset = startCountOffset + segCountX2;
    uint32_t idRangeOffsetOffset = idDeltaOffset + segCountX2;

    for (uint16_t i = 0; i < segCount; ++i) {
        uint16_t endCount = readUInt16(fontData.data() + endCountOffset + i * 2);
        uint16_t startCount = readUInt16(fontData.data() + startCountOffset + i * 2);
        int16_t idDelta = readInt16(fontData.data() + idDeltaOffset + i * 2);
        uint16_t idRangeOffset = readUInt16(fontData.data() + idRangeOffsetOffset + i * 2);

        if (startCount == 0xFFFF && endCount == 0xFFFF) {
            break; // Last segment
        }

        for (uint32_t charCode = startCount; charCode <= endCount; ++charCode) {
            uint16_t glyphIndex = 0;

            if (idRangeOffset == 0) {
                glyphIndex = (charCode + idDelta) & 0xFFFF;
            } else {
                uint32_t glyphOffset = idRangeOffsetOffset + i * 2 + idRangeOffset + 
                                     (charCode - startCount) * 2;
                if (glyphOffset + 2 <= fontData.size()) {
                    glyphIndex = readUInt16(fontData.data() + glyphOffset);
                    if (glyphIndex != 0) {
                        glyphIndex = (glyphIndex + idDelta) & 0xFFFF;
                    }
                }
            }

            if (glyphIndex != 0) {
                charToGlyph[charCode] = glyphIndex;
                glyphToChar[glyphIndex].insert(charCode);
            }
        }
    }

    std::cout << "CMAP: Format 4 parsed, " << charToGlyph.size() << " mappings" << std::endl;
}

void CMAPParser::parseFormat6(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 8 > fontData.size()) {
        std::cerr << "CMAP: Format 6 table too small" << std::endl;
        return;
    }

    // Read but don't use these variables to avoid warnings
    (void)readUInt16(fontData.data() + offset + 2); // length
    (void)readUInt16(fontData.data() + offset + 4); // language
    (void)platformID;
    (void)encodingID;

    uint16_t firstCode = readUInt16(fontData.data() + offset + 6);
    uint16_t entryCount = readUInt16(fontData.data() + offset + 8);

    if (offset + 10 + entryCount * 2 > fontData.size()) {
        std::cerr << "CMAP: Format 6 table extends beyond font data" << std::endl;
        return;
    }

    for (uint16_t i = 0; i < entryCount; ++i) {
        uint16_t glyphIndex = readUInt16(fontData.data() + offset + 10 + i * 2);
        if (glyphIndex != 0) {
            uint32_t charCode = firstCode + i;
            charToGlyph[charCode] = glyphIndex;
            glyphToChar[glyphIndex].insert(charCode);
        }
    }

    std::cout << "CMAP: Format 6 parsed, " << charToGlyph.size() << " mappings" << std::endl;
}

void CMAPParser::parseFormat12(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 16 > fontData.size()) {
        std::cerr << "CMAP: Format 12 table too small" << std::endl;
        return;
    }

    // Read but don't use these variables to avoid warnings
    (void)readUInt32(fontData.data() + offset + 4); // length
    (void)readUInt32(fontData.data() + offset + 8); // language
    (void)platformID;
    (void)encodingID;

    uint32_t numGroups = readUInt32(fontData.data() + offset + 12);

    if (offset + 16 + numGroups * 12 > fontData.size()) {
        std::cerr << "CMAP: Format 12 table extends beyond font data" << std::endl;
        return;
    }

    for (uint32_t i = 0; i < numGroups; ++i) {
        uint32_t groupOffset = offset + 16 + i * 12;
        uint32_t startCharCode = readUInt32(fontData.data() + groupOffset);
        uint32_t endCharCode = readUInt32(fontData.data() + groupOffset + 4);
        uint32_t startGlyphID = readUInt32(fontData.data() + groupOffset + 8);

        for (uint32_t charCode = startCharCode; charCode <= endCharCode; ++charCode) {
            uint32_t glyphIndex = startGlyphID + (charCode - startCharCode);
            charToGlyph[charCode] = static_cast<uint16_t>(glyphIndex);
            glyphToChar[static_cast<uint16_t>(glyphIndex)].insert(charCode);
        }
    }

    std::cout << "CMAP: Format 12 parsed, " << charToGlyph.size() << " mappings" << std::endl;
}

void CMAPParser::parseFormat13(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 16 > fontData.size()) {
        std::cerr << "CMAP: Format 13 table too small" << std::endl;
        return;
    }

    // Read but don't use these variables to avoid warnings
    (void)readUInt32(fontData.data() + offset + 4); // length
    (void)readUInt32(fontData.data() + offset + 8); // language
    (void)platformID;
    (void)encodingID;

    uint32_t numGroups = readUInt32(fontData.data() + offset + 12);

    if (offset + 16 + numGroups * 12 > fontData.size()) {
        std::cerr << "CMAP: Format 13 table extends beyond font data" << std::endl;
        return;
    }

    for (uint32_t i = 0; i < numGroups; ++i) {
        uint32_t groupOffset = offset + 16 + i * 12;
        uint32_t startCharCode = readUInt32(fontData.data() + groupOffset);
        uint32_t endCharCode = readUInt32(fontData.data() + groupOffset + 4);
        uint32_t glyphID = readUInt32(fontData.data() + groupOffset + 8);

        for (uint32_t charCode = startCharCode; charCode <= endCharCode; ++charCode) {
            charToGlyph[charCode] = static_cast<uint16_t>(glyphID);
            glyphToChar[static_cast<uint16_t>(glyphID)].insert(charCode);
        }
    }

    std::cout << "CMAP: Format 13 parsed, " << charToGlyph.size() << " mappings" << std::endl;
}

void CMAPParser::parseFormat2(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)offset;
    (void)platformID;
    (void)encodingID;
    std::cout << "CMAP: Format 2 not implemented" << std::endl;
}

void CMAPParser::parseFormat8(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)offset;
    (void)platformID;
    (void)encodingID;
    std::cout << "CMAP: Format 8 not implemented" << std::endl;
}

void CMAPParser::parseFormat10(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)offset;
    (void)platformID;
    (void)encodingID;
    std::cout << "CMAP: Format 10 not implemented" << std::endl;
}

void CMAPParser::parseFormat14(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)offset;
    (void)platformID;
    (void)encodingID;
    std::cout << "CMAP: Format 14 not implemented" << std::endl;
}

uint16_t CMAPParser::readUInt16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

uint32_t CMAPParser::readUInt32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) | 
           (static_cast<uint32_t>(data[1]) << 16) | 
           (static_cast<uint32_t>(data[2]) << 8) | 
           data[3];
}

int16_t CMAPParser::readInt16(const uint8_t* data) {
    return (static_cast<int16_t>(data[0]) << 8) | data[1];
}

} // namespace utils
} // namespace fontmaster
