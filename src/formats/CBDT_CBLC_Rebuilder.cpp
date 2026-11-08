#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <algorithm>

namespace fontmaster {
namespace {
void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.at(offset + i) = value >> (24 - i * 8);
}
void put16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    bytes.at(offset) = value >> 8;
    bytes.at(offset + 1) = value;
}
void append32(std::vector<uint8_t>& bytes, uint32_t value) {
    const auto offset = bytes.size();
    bytes.resize(offset + 4);
    put32(bytes, offset, value);
}
}

std::vector<uint8_t> CBDT_CBLC_Rebuilder::rebuild() {
    std::vector<uint8_t> cblc(8 + strikes.size() * 48, 0), cbdt(4, 0);
    put32(cblc, 0, 0x00030000);
    put32(cblc, 4, strikes.size());
    put32(cbdt, 0, 0x00030000);
    size_t strikeIndex = 0;
    for (const auto& entry : strikes) {
        const auto& strike = entry.second;
        const auto header = 8 + strikeIndex++ * 48;
        if (strike.bitmapSizeTable.size() == 48) {
            std::copy(strike.bitmapSizeTable.begin(), strike.bitmapSizeTable.end(), cblc.begin() + header);
        } else {
            cblc[header + 44] = strike.ppem;
            cblc[header + 45] = strike.ppem;
            cblc[header + 46] = 32;
            cblc[header + 47] = 1;
        }
        // Each run has a contiguous glyph range and one image format.
        std::vector<std::vector<const GlyphImage*>> runs;
        for (const auto& item : strike.glyphImages) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), item.first) != removedGlyphs.end()) continue;
            const auto& image = item.second;
            if (image.recordData.empty()) throw std::runtime_error("Missing CBDT image record");
            if (runs.empty() || uint32_t(runs.back().back()->glyphID) + 1 != image.glyphID ||
                runs.back().back()->imageFormat != image.imageFormat) runs.emplace_back();
            runs.back().push_back(&image);
        }
        const auto arrayStart = cblc.size();
        cblc.resize(arrayStart + runs.size() * 8, 0);
        for (size_t i = 0; i < runs.size(); ++i) {
            const auto& run = runs[i];
            put16(cblc, arrayStart + i * 8, run.front()->glyphID);
            put16(cblc, arrayStart + i * 8 + 2, run.back()->glyphID);
            put32(cblc, arrayStart + i * 8 + 4, cblc.size() - arrayStart);
            const auto subtable = cblc.size();
            cblc.resize(subtable + 8, 0);
            put16(cblc, subtable, 1);
            put16(cblc, subtable + 2, run.front()->imageFormat);
            put32(cblc, subtable + 4, cbdt.size());
            uint32_t offset = 0;
            for (const auto* image : run) {
                append32(cblc, offset);
                cbdt.insert(cbdt.end(), image->recordData.begin(), image->recordData.end());
                offset += image->recordData.size();
            }
            append32(cblc, offset); // Required terminal offset.
        }
        put32(cblc, header, arrayStart);
        put32(cblc, header + 4, cblc.size() - arrayStart);
        put32(cblc, header + 8, runs.size());
        put16(cblc, header + 40, runs.empty() ? 0 : runs.front().front()->glyphID);
        put16(cblc, header + 42, runs.empty() ? 0 : runs.back().back()->glyphID);
    }
    return utils::rebuildSfnt(fontData, {{"CBLC", cblc}, {"CBDT", cbdt}});
}
} // namespace fontmaster
