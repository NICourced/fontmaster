#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <iostream>
#include <algorithm>
#include <cstring>

namespace fontmaster {

using namespace fontmaster::utils;

static inline void appendUInt32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back((value >> 24) & 0xFF);
    data.push_back((value >> 16) & 0xFF);
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

static inline void appendUInt16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

static inline void setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset + 4 <= data.size()) {
        data[offset] = (value >> 24) & 0xFF;
        data[offset + 1] = (value >> 16) & 0xFF;
        data[offset + 2] = (value >> 8) & 0xFF;
        data[offset + 3] = value & 0xFF;
    }
}

static inline uint32_t calcTableChecksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    for (size_t i = 0; i < data.size(); i += 4) {
        uint32_t val = 0;
        for (int j = 0; j < 4 && i + j < data.size(); ++j)
            val = (val << 8) | data[i + j];
        sum += val;
    }
    return sum;
}

static inline void setTag(std::vector<uint8_t>& buf, size_t offset, const std::string& tag) {
    for (size_t i = 0; i < 4; ++i)
        buf[offset + i] = (i < tag.size()) ? tag[i] : ' ';
}

/* ────────────────────────────────  MAIN  ─────────────────────────────── */

std::vector<uint8_t> CBDT_CBLC_Rebuilder::rebuild() {
    std::vector<uint8_t> cblcData;
    std::vector<uint8_t> cbdtData;

    rebuildCBLCTable(cblcData);
    rebuildCBDTTable(cbdtData);

    return createUpdatedFont(cblcData, cbdtData);
}

/* ─────────────────────────────── CBLC ─────────────────────────────── */

void CBDT_CBLC_Rebuilder::rebuildCBLCTable(std::vector<uint8_t>& cblc) {
    appendUInt32(cblc, 0x00020000); // version
    appendUInt32(cblc, static_cast<uint32_t>(strikes.size())); // numStrikes

    std::vector<uint32_t> strikeOffsets(strikes.size(), 0);
    size_t offsetStart = cblc.size();
    cblc.resize(cblc.size() + strikes.size() * 4, 0);

    size_t idx = 0;
    for (const auto& [id, strike] : strikes) {
        strikeOffsets[idx] = cblc.size();
        rebuildStrike(cblc, strike);
        ++idx;
    }

    for (size_t i = 0; i < strikeOffsets.size(); ++i)
        setUInt32(cblc, offsetStart + i * 4, strikeOffsets[i]);
}

void CBDT_CBLC_Rebuilder::rebuildStrike(std::vector<uint8_t>& buf, const StrikeRecord& strike) {
    appendUInt16(buf, strike.ppem);
    appendUInt16(buf, strike.resolution);
    appendUInt32(buf, 0); // colorRef
    appendUInt16(buf, 72);
    appendUInt16(buf, 72);

    uint16_t firstGlyph = 0xFFFF, lastGlyph = 0;
    for (uint16_t g : strike.glyphIDs) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), g) == removedGlyphs.end()) {
            firstGlyph = std::min(firstGlyph, g);
            lastGlyph  = std::max(lastGlyph, g);
        }
    }
    if (firstGlyph == 0xFFFF) firstGlyph = lastGlyph = 0;

    appendUInt16(buf, firstGlyph);
    appendUInt16(buf, lastGlyph);
    appendUInt16(buf, strike.ppem);
    appendUInt16(buf, strike.ppem);
    buf.push_back(1); // bitDepth
    appendUInt16(buf, 0); // flags

    appendUInt32(buf, 0x00000020); // offsetArray (placeholder)
    appendUInt32(buf, 1);          // numSubtables
    appendUInt32(buf, 0);          // colorRef

    rebuildIndexSubtable1(buf, strike, firstGlyph, lastGlyph);
}

void CBDT_CBLC_Rebuilder::rebuildIndexSubtable1(
    std::vector<uint8_t>& buf, const StrikeRecord& strike,
    uint16_t firstGlyph, uint16_t lastGlyph
) {
    appendUInt16(buf, firstGlyph);
    appendUInt16(buf, lastGlyph);
    appendUInt16(buf, 1);   // indexFormat
    appendUInt16(buf, 17);  // imageFormat = PNG(small metrics)
    appendUInt32(buf, 0);   // imageDataOffsetArray offset placeholder

    uint32_t imgDataOffset = 0;
    size_t offsetArrayPos = buf.size();
    for (uint16_t g = firstGlyph; g <= lastGlyph; ++g) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), g) != removedGlyphs.end()) {
            appendUInt32(buf, 0);
            continue;
        }
        auto it = strike.glyphImages.find(g);
        if (it != strike.glyphImages.end() && !it->second.data.empty()) {
            appendUInt32(buf, imgDataOffset);
            imgDataOffset += it->second.data.size();
        } else {
            appendUInt32(buf, 0);
        }
    }
    setUInt32(buf, offsetArrayPos - 4, static_cast<uint32_t>(offsetArrayPos - offsetArrayPos + 4));
}

/* ─────────────────────────────── CBDT ─────────────────────────────── */

void CBDT_CBLC_Rebuilder::rebuildCBDTTable(std::vector<uint8_t>& cbdt) {
    appendUInt32(cbdt, 0x00020000); // version

    for (const auto& [id, strike] : strikes) {
        for (uint16_t g : strike.glyphIDs) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), g) != removedGlyphs.end())
                continue;
            auto it = strike.glyphImages.find(g);
            if (it != strike.glyphImages.end())
                cbdt.insert(cbdt.end(), it->second.data.begin(), it->second.data.end());
        }
    }
}

/* ─────────────────────────────── FONT UPDATE ─────────────────────────────── */

std::vector<uint8_t> CBDT_CBLC_Rebuilder::createUpdatedFont(
    const std::vector<uint8_t>& newCBLCTable,
    const std::vector<uint8_t>& newCBDTTable
) {
    auto tables = parseTTFTables(fontData);
    std::vector<uint8_t> newFont;

    const TTFHeader* header = reinterpret_cast<const TTFHeader*>(fontData.data());
    uint16_t numTables = header->numTables;

    appendUInt32(newFont, header->sfntVersion);
    appendUInt16(newFont, numTables);
    appendUInt16(newFont, header->searchRange);
    appendUInt16(newFont, header->entrySelector);
    appendUInt16(newFont, header->rangeShift);

    size_t dirStart = newFont.size();
    newFont.resize(dirStart + numTables * sizeof(TableRecord));

    size_t currentOffset = 12 + numTables * 16;
    for (size_t i = 0; i < tables.size(); ++i) {
        const TableRecord& t = tables[i];
        std::string tag(t.tag, t.tag + 4);

        std::vector<uint8_t> data;
        if (tag == "CBLC")
            data = newCBLCTable;
        else if (tag == "CBDT")
            data = newCBDTTable;
        else
            data.assign(fontData.begin() + t.offset, fontData.begin() + t.offset + t.length);

        while (data.size() % 4 != 0) data.push_back(0);
        uint32_t checksum = calcTableChecksum(data);

        // Write table record
        setTag(newFont, dirStart + i * 16, tag);
        setUInt32(newFont, dirStart + i * 16 + 4, checksum);
        setUInt32(newFont, dirStart + i * 16 + 8, currentOffset);
        setUInt32(newFont, dirStart + i * 16 + 12, data.size());

        newFont.insert(newFont.end(), data.begin(), data.end());
        currentOffset += data.size();
    }

    // Fix head checksum
    const TableRecord* head = findTable(tables, "head");
    if (head) {
        size_t headOffset = head->offset;
        setUInt32(newFont, headOffset + 8, 0);
        uint32_t total = calcTableChecksum(newFont);
        uint32_t adjust = 0xB1B0AFBA - total;
        setUInt32(newFont, headOffset + 8, adjust);
    }

    std::cout << "[Rebuilder] Created font with "
              << newCBLCTable.size() << " bytes (CBLC) and "
              << newCBDTTable.size() << " bytes (CBDT)\n";

    return newFont;
}

} // namespace fontmaster

