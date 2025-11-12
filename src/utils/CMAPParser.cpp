#ifndef CMAPPARSER_H
#define CMAPPARSER_H

#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>

namespace fontmaster {
namespace utils {

struct CMAPRange {
    uint32_t startChar;
    uint32_t endChar;
    uint16_t startGlyph;
};

class CMAPParser {
public:
    CMAPParser(const std::vector<uint8_t>& fontData, bool verbose = false)
        : fontData(fontData), verbose(verbose) {}

    void parse() {
        if (fontData.size() < 4)
            throw std::runtime_error("CMAP: Font data too small");

        uint16_t version = readUInt16(fontData.data());
        uint16_t numTables = readUInt16(fontData.data() + 2);

        if (version != 0)
            throw std::runtime_error("CMAP: Unsupported version: " + std::to_string(version));

        if (verbose) std::cout << "CMAP: Parsing " << numTables << " subtables\n";

        for (uint16_t i = 0; i < numTables; ++i) {
            uint32_t tableOffset = 4 + i * 8;
            if (tableOffset + 8 > fontData.size())
                throw std::runtime_error("CMAP: Table record out of bounds");

            uint16_t platformID = readUInt16(fontData.data() + tableOffset);
            uint16_t encodingID = readUInt16(fontData.data() + tableOffset + 2);
            uint32_t subtableOffset = readUInt32(fontData.data() + tableOffset + 4);

            if (subtableOffset >= fontData.size())
                throw std::runtime_error("CMAP: Subtable offset out of bounds");

            parseSubtable(subtableOffset, platformID, encodingID);
        }
    }

    uint16_t getGlyphIndex(uint32_t charCode) const {
        auto it = charToGlyph.find(charCode);
        if (it != charToGlyph.end()) return it->second;

        // Быстрый поиск в диапазонах
        for (const auto& range : ranges) {
            if (charCode >= range.startChar && charCode <= range.endChar) {
                return static_cast<uint16_t>(range.startGlyph + (charCode - range.startChar));
            }
        }
        return 0;
    }

    std::set<uint32_t> getCharCodes(uint16_t glyphIndex) const {
        std::set<uint32_t> codes;
        auto it = glyphToChar.find(glyphIndex);
        if (it != glyphToChar.end()) codes = it->second;

        for (const auto& range : ranges) {
            if (glyphIndex >= range.startGlyph && glyphIndex <= range.startGlyph + (range.endChar - range.startChar)) {
                for (uint32_t c = range.startChar; c <= range.endChar; ++c) {
                    if (range.startGlyph + (c - range.startChar) == glyphIndex)
                        codes.insert(c);
                }
            }
        }
        return codes;
    }

private:
    const std::vector<uint8_t>& fontData;
    std::map<uint32_t, uint16_t> charToGlyph;
    std::map<uint16_t, std::set<uint32_t>> glyphToChar;
    std::vector<CMAPRange> ranges;
    bool verbose;

    void parseSubtable(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        if (offset + 2 > fontData.size()) return;
        uint16_t format = readUInt16(fontData.data() + offset);

        switch (format) {
            case 0: parseFormat0(offset); break;
            case 2: parseFormat2(offset); break;
            case 4: parseFormat4(offset); break;
            case 6: parseFormat6(offset); break;
            case 8: parseFormat8(offset); break;
            case 10: parseFormat10(offset); break;
            case 12: parseFormat12(offset); break;
            case 13: parseFormat13(offset); break;
            case 14: parseFormat14(offset); break;
            default:
                if (verbose) std::cout << "CMAP: Unsupported format: " << format << "\n";
                break;
        }
    }

    // ======================= Форматы =========================
    void parseFormat0(uint32_t offset) {
        if (offset + 6 + 256 > fontData.size()) throw std::runtime_error("CMAP: Format 0 too small");
        for (int i = 0; i < 256; ++i) {
            uint8_t g = fontData[offset + 6 + i];
            if (g != 0) {
                charToGlyph[i] = g;
                glyphToChar[g].insert(i);
            }
        }
        if (verbose) std::cout << "CMAP: Format 0 parsed\n";
    }

    void parseFormat2(uint32_t offset) {
        if (verbose) std::cout << "CMAP: Format 2 (high-byte mapping) not fully implemented\n";
    }

    void parseFormat4(uint32_t offset) {
        if (offset + 14 > fontData.size()) throw std::runtime_error("CMAP: Format 4 too small");
        uint16_t segCountX2 = readUInt16Safe(offset + 6);
        uint16_t segCount = segCountX2 / 2;

        uint32_t endCountOffset = offset + 14;
        uint32_t startCountOffset = endCountOffset + segCountX2 + 2;
        uint32_t idDeltaOffset = startCountOffset + segCountX2;
        uint32_t idRangeOffsetOffset = idDeltaOffset + segCountX2;

        for (uint16_t i = 0; i < segCount; ++i) {
            uint16_t endCount = readUInt16Safe(endCountOffset + i * 2);
            uint16_t startCount = readUInt16Safe(startCountOffset + i * 2);
            int16_t idDelta = readInt16Safe(idDeltaOffset + i * 2);
            uint16_t idRangeOffset = readUInt16Safe(idRangeOffsetOffset + i * 2);

            if (startCount == 0xFFFF && endCount == 0xFFFF) break;

            for (uint32_t c = startCount; c <= endCount; ++c) {
                uint16_t glyph = 0;
                if (idRangeOffset == 0) {
                    glyph = (c + idDelta) & 0xFFFF;
                } else {
                    uint32_t glyphOffset = idRangeOffsetOffset + i * 2 + idRangeOffset + (c - startCount) * 2;
                    if (glyphOffset + 2 <= fontData.size()) {
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
        if (verbose) std::cout << "CMAP: Format 4 parsed\n";
    }

    void parseFormat6(uint32_t offset) {
        uint16_t firstCode = readUInt16Safe(offset + 6);
        uint16_t entryCount = readUInt16Safe(offset + 8);
        for (uint16_t i = 0; i < entryCount; ++i) {
            uint16_t g = readUInt16Safe(offset + 10 + i * 2);
            if (g != 0) {
                charToGlyph[firstCode + i] = g;
                glyphToChar[g].insert(firstCode + i);
            }
        }
        if (verbose) std::cout << "CMAP: Format 6 parsed\n";
    }

    void parseFormat8(uint32_t offset) {
        if (verbose) std::cout << "CMAP: Format 8 (mixed 16/32-bit) not implemented\n";
    }

    void parseFormat10(uint32_t offset) {
        if (verbose) std::cout << "CMAP: Format 10 (32-bit) not implemented\n";
    }

    void parseFormat12(uint32_t offset) {
        uint32_t numGroups = readUInt32Safe(offset + 12);
        for (uint32_t i = 0; i < numGroups; ++i) {
            uint32_t groupOffset = offset + 16 + i * 12;
            uint32_t startChar = readUInt32Safe(groupOffset);
            uint32_t endChar = readUInt32Safe(groupOffset + 4);
            uint16_t startGlyph = static_cast<uint16_t>(readUInt32Safe(groupOffset + 8));

            ranges.push_back({startChar, endChar, startGlyph});
        }
        if (verbose) std::cout << "CMAP: Format 12 parsed\n";
    }

    void parseFormat13(uint32_t offset) {
        uint32_t numGroups = readUInt32Safe(offset + 12);
        for (uint32_t i = 0; i < numGroups; ++i) {
            uint32_t groupOffset = offset + 16 + i * 12;
            uint32_t startChar = readUInt32Safe(groupOffset);
            uint32_t endChar = readUInt32Safe(groupOffset + 4);
            uint16_t glyph = static_cast<uint16_t>(readUInt32Safe(groupOffset + 8));
            for (uint32_t c = startChar; c <= endChar; ++c) {
                charToGlyph[c] = glyph;
                glyphToChar[glyph].insert(c);
            }
        }
        if (verbose) std::cout << "CMAP: Format 13 parsed\n";
    }

    void parseFormat14(uint32_t offset) {
        if (verbose) std::cout << "CMAP: Format 14 (variation selectors) not implemented\n";
    }

    // ======================= Чтение данных =========================
    uint16_t readUInt16(const uint8_t* data) { return (data[0] << 8) | data[1]; }
    uint32_t readUInt32(const uint8_t* data) { return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]; }
    int16_t readInt16(const uint8_t* data) { return (int16_t)((data[0] << 8) | data[1]); }

    uint16_t readUInt16Safe(uint32_t offset) {
        if (offset + 2 > fontData.size()) throw std::runtime_error("CMAP: readUInt16Safe out of bounds");
        return readUInt16(fontData.data() + offset);
    }

    uint32_t readUInt32Safe(uint32_t offset) {
        if (offset + 4 > fontData.size()) throw std::runtime_error("CMAP: readUInt32Safe out of bounds");
        return readUInt32(fontData.data() + offset);
    }

    int16_t readInt16Safe(uint32_t offset) {
        if (offset + 2 > fontData.size()) throw std::runtime_error("CMAP: readInt16Safe out of bounds");
        return readInt16(fontData.data() + offset);
    }
};

} // namespace utils
} // namespace fontmaster

#endif // CMAPPARSER_H
