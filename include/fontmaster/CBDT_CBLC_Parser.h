#pragma once
#include "fontmaster/CBDT_CBLC_Types.h"

namespace fontmaster {
class CBDT_CBLC_Parser {
public:
    explicit CBDT_CBLC_Parser(const std::vector<uint8_t>& data) : fontData(data) {}
    bool parse();
    bool removeGlyph(uint16_t glyphID);
    bool replaceGlyphImage(uint16_t glyphID, const std::vector<uint8_t>& png);
    const std::map<uint16_t, StrikeRecord>& getStrikes() const { return strikes; }
    // Only explicit edits are removals: ligatures need not occur in cmap.
    const std::vector<uint16_t>& getRemovedGlyphs() const { return removedGlyphs; }
private:
    const std::vector<uint8_t>& fontData;
    std::map<uint16_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;
};
} // namespace fontmaster
