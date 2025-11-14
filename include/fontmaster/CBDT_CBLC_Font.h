#ifndef CBDT_CBLC_FONT_H
#define CBDT_CBLC_FONT_H

#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include <string>
#include <vector>

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
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override;
    std::string findGlyphName(uint32_t unicode) const override;
    // CBDT/CBLC specific methods
    const std::map<uint16_t, StrikeRecord>& getStrikes() const { return parser.getStrikes(); }
    const std::vector<uint16_t>& getRemovedGlyphs() const { return parser.getRemovedGlyphs(); }
    
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    CBDT_CBLC_Parser parser;
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
