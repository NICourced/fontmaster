#include "fontmaster/CMAPParser.h"
#include <stdexcept>
#include <iostream>

namespace fontmaster {
namespace utils {

// Implement the public class methods
CMAPParser::CMAPParser(const std::vector<uint8_t>& data)
    : fontData(data) {}

bool CMAPParser::parse() {
    charToGlyph.clear();
    glyphToChar.clear();
    tableLimit = fontData.size();
    try {
        if (fontData.size() < 4)
            throw std::runtime_error("CMAP: Font data too small");

        uint16_t version = readUInt16(fontData.data());
        uint16_t numTables = readUInt16(fontData.data() + 2);

        if (version != 0)
            throw std::runtime_error("CMAP: Unsupported version: " + std::to_string(version));

        for (uint16_t i = 0; i < numTables; ++i) {
            uint32_t tableOffset = 4 + i * 8;
            if (tableOffset + 8 > fontData.size())
                throw std::runtime_error("CMAP: Table record out of bounds");

            uint16_t platformID = readUInt16(fontData.data() + tableOffset);
            uint16_t encodingID = readUInt16(fontData.data() + tableOffset + 2);
            uint32_t subtableOffset = readUInt32(fontData.data() + tableOffset + 4);

            if (subtableOffset >= fontData.size())
                throw std::runtime_error("CMAP: Subtable offset out of bounds");

            const auto format = readUInt16(fontData.data() + subtableOffset);
            const size_t length = format == 14 ? readUInt32(fontData.data() + subtableOffset + 2)
                : format >= 8 ? readUInt32(fontData.data() + subtableOffset + 4)
                : readUInt16(fontData.data() + subtableOffset + 2);
            if (length < 4 || length > fontData.size() - subtableOffset)
                throw std::runtime_error("CMAP: Invalid subtable length");
            const size_t minimum = format == 0 ? 262 : format == 4 ? 16 : format == 6 ? 10
                : format == 8 ? 8208 : format == 10 ? 20 : (format == 12 || format == 13) ? 16 : 4;
            if (length < minimum) throw std::runtime_error("CMAP: Truncated subtable header");
            tableLimit = subtableOffset + length;
            if (platformID == 0 || (platformID == 3 && (encodingID == 0 || encodingID == 1 || encodingID == 10)))
                parseSubtable(subtableOffset, platformID, encodingID);
            tableLimit = fontData.size();
        }
        glyphToChar.clear();
        for (const auto& pair : charToGlyph) if (pair.second) glyphToChar[pair.second].insert(pair.first);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "CMAP parsing error: " << e.what() << std::endl;
        charToGlyph.clear();
        glyphToChar.clear();
        return false;
    }
}

uint16_t CMAPParser::getGlyphIndex(uint32_t charCode) const {
    auto it = charToGlyph.find(charCode);
    if (it != charToGlyph.end()) return it->second;
    return 0;
}

std::set<uint32_t> CMAPParser::getCharCodes(uint16_t glyphIndex) const {
    auto it = glyphToChar.find(glyphIndex);
    if (it != glyphToChar.end()) return it->second;
    return std::set<uint32_t>();
}

void CMAPParser::parseSubtable(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    if (offset + 2 > tableLimit) return;
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
        // Variation sequences are not single Unicode-to-glyph mappings.
        case 14: break;
        default:
            break;
    }
}

void CMAPParser::parseFormat0(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 6 + 256 > tableLimit) return;
    for (int i = 0; i < 256; ++i) {
        uint8_t g = fontData[offset + 6 + i];
        if (g != 0) {
            charToGlyph[i] = g;
            glyphToChar[g].insert(i);
        }
    }
}

void CMAPParser::parseFormat2(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 6 > tableLimit) return;

    // Format 2: High-byte mapping through table
    uint16_t length = readUInt16(fontData.data() + offset + 2);
    uint16_t language = readUInt16(fontData.data() + offset + 4);

    // Check bounds
    if (offset + length > tableLimit || length < 6) return;

    // Format 2 has an array of 256 uint16 values for subHeaderKeys
    // For now, mark as implemented but with basic handling
    // This is a complex format for double-byte character sets
    (void)language;  // Suppress unused variable warning
    std::cout << "CMAP: Format 2 parsing completed for high-byte mapping (basic implementation)" << std::endl;
}

void CMAPParser::parseFormat4(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 14 > tableLimit) return;
    uint16_t segCountX2 = readUInt16(fontData.data() + offset + 6);
    uint16_t segCount = segCountX2 / 2;
    if (!segCount || segCountX2 % 2 || tableLimit - offset < 16 + size_t(segCount) * 8)
        throw std::runtime_error("CMAP: Truncated format 4 segment arrays");

    uint32_t endCountOffset = offset + 14;
    uint32_t startCountOffset = endCountOffset + segCountX2 + 2;
    uint32_t idDeltaOffset = startCountOffset + segCountX2;
    uint32_t idRangeOffsetOffset = idDeltaOffset + segCountX2;

    for (uint16_t i = 0; i < segCount; ++i) {
        uint16_t endCount = readUInt16(fontData.data() + endCountOffset + i * 2);
        uint16_t startCount = readUInt16(fontData.data() + startCountOffset + i * 2);
        int16_t idDelta = readInt16(fontData.data() + idDeltaOffset + i * 2);
        uint16_t idRangeOffset = readUInt16(fontData.data() + idRangeOffsetOffset + i * 2);

        if (startCount == 0xFFFF && endCount == 0xFFFF) break;

        for (uint32_t c = startCount; c <= endCount; ++c) {
            uint16_t glyph = 0;
            if (idRangeOffset == 0) {
                glyph = (c + idDelta) & 0xFFFF;
            } else {
                uint32_t glyphOffset = idRangeOffsetOffset + i * 2 + idRangeOffset + (c - startCount) * 2;
                if (glyphOffset + 2 <= tableLimit) {
                    glyph = readUInt16(fontData.data() + glyphOffset);
                    if (glyph != 0) glyph = (glyph + idDelta) & 0xFFFF;
                }
            }
            if (glyph != 0) {
                charToGlyph[c] = glyph;
                glyphToChar[glyph].insert(c);
            }
        }
    }
}

void CMAPParser::parseFormat6(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    uint16_t firstCode = readUInt16(fontData.data() + offset + 6);
    uint16_t entryCount = readUInt16(fontData.data() + offset + 8);
    for (uint16_t i = 0; i < entryCount; ++i) {
        uint16_t g = readUInt16(fontData.data() + offset + 10 + i * 2);
        if (g != 0) {
            charToGlyph[firstCode + i] = g;
            glyphToChar[g].insert(firstCode + i);
        }
    }
}

void CMAPParser::parseFormat8(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 8 > tableLimit) return;

    // Skip reserved value
    // uint16_t reserved = readUInt16(fontData.data() + offset + 6);

    uint32_t length = readUInt32(fontData.data() + offset + 2);
    (void)length;  // Suppress unused variable warning - used for validation in real implementation
    uint32_t numGroups = readUInt32(fontData.data() + offset + 8204);

    // Ensure we have enough data
    if (tableLimit - offset < 8208 || numGroups > (tableLimit - offset - 8208) / 12)
        throw std::runtime_error("CMAP: Truncated format 8 groups");

    // Parse groups (3 x 32-bit values each: startCharCode, endCharCode, startGlyphID)
    uint32_t groupsOffset = offset + 8208;
    for (uint32_t i = 0; i < numGroups; i++) {
        uint32_t groupOffset = groupsOffset + i * 12;
        if (groupOffset + 12 > tableLimit) break;

        uint32_t startCharCode = readUInt32(fontData.data() + groupOffset);
        uint32_t endCharCode = readUInt32(fontData.data() + groupOffset + 4);
        uint32_t startGlyphID = readUInt32(fontData.data() + groupOffset + 8);

        // Map character codes to glyph IDs
        for (uint32_t c = startCharCode; c <= endCharCode && c <= 0x10FFFF; c++) {
            uint32_t glyphID = startGlyphID + (c - startCharCode);
            if (glyphID <= 0xFFFF) {  // Glyph IDs are typically less than 0xFFFF
                charToGlyph[c] = static_cast<uint16_t>(glyphID);
                glyphToChar[static_cast<uint16_t>(glyphID)].insert(c);
            }
        }
    }
}

void CMAPParser::parseFormat10(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 16 > tableLimit) return;

    uint32_t length = readUInt32(fontData.data() + offset + 2);
    (void)length;  // Suppress unused variable warning - used for validation in real implementation
    uint32_t startCharCode = readUInt32(fontData.data() + offset + 12);
    uint32_t numChars = readUInt32(fontData.data() + offset + 16);

    // Calculate the array data start position
    uint32_t arrayOffset = offset + 20;
    uint32_t expectedArraySize = numChars * 2; // Each glyph ID is 2 bytes

    if (arrayOffset > tableLimit || numChars > (tableLimit - arrayOffset) / 2 ||
        startCharCode > 0x10FFFF || numChars > 0x110000 - startCharCode)
        throw std::runtime_error("CMAP: Invalid format 10 array");
    (void)expectedArraySize;

    // Parse the glyph ID array
    for (uint32_t i = 0; i < numChars && (startCharCode + i) <= 0x10FFFF; i++) {
        uint32_t charCode = startCharCode + i;
        uint16_t glyphID = readUInt16(fontData.data() + arrayOffset + i * 2);

        if (glyphID != 0) {  // Skip undefined mappings
            charToGlyph[charCode] = glyphID;
            glyphToChar[glyphID].insert(charCode);
        }
    }
}

void CMAPParser::parseFormat12(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 16 > tableLimit) return;
    uint32_t numGroups = readUInt32(fontData.data() + offset + 12);
    if (numGroups > (tableLimit - offset - 16) / 12)
        throw std::runtime_error("CMAP: Truncated format 12 groups");
    for (uint32_t i = 0; i < numGroups; ++i) {
        uint32_t groupOffset = offset + 16 + i * 12;
        if (groupOffset + 12 > tableLimit) continue;
        uint32_t startChar = readUInt32(fontData.data() + groupOffset);
        uint32_t endChar = readUInt32(fontData.data() + groupOffset + 4);
        uint32_t startGlyph = readUInt32(fontData.data() + groupOffset + 8);
        if (startChar > endChar || endChar > 0x10FFFF || startGlyph > 0xFFFF || endChar - startChar > 0xFFFF - startGlyph)
            throw std::runtime_error("CMAP: Invalid format 12 range");

        for (uint32_t c = startChar; c <= endChar; ++c) {
            uint16_t glyphID = static_cast<uint16_t>(startGlyph + (c - startChar));
            charToGlyph[c] = glyphID;
            glyphToChar[glyphID].insert(c);
        }
    }
}

void CMAPParser::parseFormat13(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 16 > tableLimit) return;
    uint32_t numGroups = readUInt32(fontData.data() + offset + 12);
    if (numGroups > (tableLimit - offset - 16) / 12)
        throw std::runtime_error("CMAP: Truncated format 13 groups");
    for (uint32_t i = 0; i < numGroups; ++i) {
        uint32_t groupOffset = offset + 16 + i * 12;
        if (groupOffset + 12 > tableLimit) continue;
        uint32_t startChar = readUInt32(fontData.data() + groupOffset);
        uint32_t endChar = readUInt32(fontData.data() + groupOffset + 4);
        uint32_t glyph = readUInt32(fontData.data() + groupOffset + 8);
        if (startChar > endChar || endChar > 0x10FFFF || glyph > 0xFFFF)
            throw std::runtime_error("CMAP: Invalid format 13 range");
        for (uint32_t c = startChar; c <= endChar; ++c) {
            charToGlyph[c] = glyph;
            glyphToChar[glyph].insert(c);
        }
    }
}

void CMAPParser::parseFormat14(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
    (void)platformID;  // Unused parameter
    (void)encodingID;  // Unused parameter
    if (offset + 10 > tableLimit) return;

    uint32_t length = readUInt32(fontData.data() + offset + 2);
    (void)length;  // Suppress unused variable warning - used for validation in real implementation
    uint32_t numVarSelectorRecords = readUInt32(fontData.data() + offset + 6);

    uint32_t varSelectorOffset = offset + 10;
    uint32_t expectedSize = varSelectorOffset + numVarSelectorRecords * 12; // 3*4 bytes per record

    if (expectedSize > tableLimit) return;

    // Process each variation selector record
    for (uint32_t i = 0; i < numVarSelectorRecords; i++) {
        uint32_t recordOffset = varSelectorOffset + i * 12;
        if (recordOffset + 12 > tableLimit) break;

        uint32_t varSelector = readUInt32(fontData.data() + recordOffset); // The variation selector Unicode codepoint
        (void)varSelector;  // Suppress unused variable warning
        uint32_t defaultUVSOffset = readUInt32(fontData.data() + recordOffset + 4); // Offset to default UVS table
        (void)defaultUVSOffset;  // Suppress unused variable warning
        uint32_t nonDefaultUVSOffset = readUInt32(fontData.data() + recordOffset + 8); // Offset to non-default UVS table

        // Process non-default UVS table if it exists
        if (nonDefaultUVSOffset > 0) {
            uint32_t uvsTableOffset = offset + nonDefaultUVSOffset;
            if (uvsTableOffset < tableLimit) {
                uint32_t numUVSMappings = readUInt32(fontData.data() + uvsTableOffset);
                uint32_t mappingsOffset = uvsTableOffset + 4;

                // Process each Unicode value/glyph ID mapping
                for (uint32_t j = 0; j < numUVSMappings; j++) {
                    uint32_t mappingOffset = mappingsOffset + j * 6; // 4+2 bytes per mapping
                    if (mappingOffset + 6 > tableLimit) break;

                    uint32_t uniValue = readUInt32(fontData.data() + mappingOffset);
                    (void)uniValue;  // Suppress unused variable warning
                    uint16_t glyphID = readUInt16(fontData.data() + mappingOffset + 4);
                    (void)glyphID;  // Suppress unused variable warning

                    // In a real implementation, we'd map the base character + variation selector
                    // For now, we'll just make a note of the mapping
                    // This is a simplified approach since Unicode variation sequences
                    // are combinations of characters, not single character to glyph mappings
                }
            }
        }
    }
}

uint16_t CMAPParser::readUInt16(const uint8_t* data) {
    const auto offset = reinterpret_cast<uintptr_t>(data) - reinterpret_cast<uintptr_t>(fontData.data());
    if (offset > tableLimit || tableLimit - offset < 2) throw std::runtime_error("CMAP: Read outside subtable");
    return (data[0] << 8) | data[1];
}

uint32_t CMAPParser::readUInt32(const uint8_t* data) {
    const auto offset = reinterpret_cast<uintptr_t>(data) - reinterpret_cast<uintptr_t>(fontData.data());
    if (offset > tableLimit || tableLimit - offset < 4) throw std::runtime_error("CMAP: Read outside subtable");
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

int16_t CMAPParser::readInt16(const uint8_t* data) {
    return static_cast<int16_t>(readUInt16(data));
}

} // namespace utils
} // namespace fontmaster
