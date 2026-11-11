#ifndef CBDT_CBLC_FONT_H
#define CBDT_CBLC_FONT_H

#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include <string>
#include <vector>
#include <memory>

namespace fontmaster {

class CBDT_CBLC_Font : public Font {
public:
    CBDT_CBLC_Font(const std::string& filepath);
    virtual ~CBDT_CBLC_Font() = default;

    bool load() override;
    bool save(const std::string& filepath) override;

    const std::vector<uint8_t>& getFontData() const override;
    void setFontData(const std::vector<uint8_t>& data) override;
    FontFormat getFormat() const override { return FontFormat::CBDT_CBLC; }
    bool removeGlyph(const std::string& glyphName) override;
    bool removeGlyph(uint32_t unicode) override;
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override;
    std::vector<GlyphInfo> listGlyphs() const override;
    std::vector<GlyphInfo> listGlyphMetadata() const override;
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override;
    std::string findGlyphName(uint32_t unicode) const override;
    // CBDT/CBLC specific methods
    const std::map<uint16_t, StrikeRecord>& getStrikes() const;
    const std::vector<uint16_t>& getRemovedGlyphs() const;

private:
    std::vector<GlyphInfo> glyphList(bool includeImageData) const;
    std::string filepath;
    std::vector<uint8_t> fontData;
    mutable std::unique_ptr<CBDT_CBLC_Parser> parser;
    mutable bool isParsed = false;
    mutable std::map<uint16_t, std::string> cachedNames;
    mutable std::map<uint32_t, uint16_t> charToGlyph;
    mutable std::map<uint16_t, uint32_t> glyphToChar;
    mutable uint16_t glyphCount = 0;
    bool ensureParsed() const;
    std::string getGlyphName(uint16_t glyphID, const std::map<uint16_t				, std::string>& postGlyphNames) const;
    std::map<uint16_t, std::string> getPostGlyphNames() const;
    uint16_t getMaxpGlyphCount() const;
    uint16_t findGlyphID(const std::string& glyphName) const;
    uint16_t findGlyphIDByUnicode(uint32_t unicode) const;
    uint32_t getUnicodeFromGlyphID(uint16_t glyphID) const;
    std::string getImageFormatString(uint16_t imageFormat) const;
};

} // namespace fontmaster

#endif // CBDT_CBLC_FONT_H
