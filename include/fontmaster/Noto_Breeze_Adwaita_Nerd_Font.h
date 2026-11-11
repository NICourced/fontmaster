#ifndef NOTO_BREEZE_ADWAITA_NERD_FONT_H
#define NOTO_BREEZE_ADWAITA_NERD_FONT_H

#include "fontmaster/FontMaster.h"
#include <string>
#include <vector>
#include <memory>

namespace fontmaster {

class Noto_Breeze_Adwaita_Nerd_Font : public Font {
private:
    std::string filepath;
    std::unique_ptr<Font> implementation;

public:
    Noto_Breeze_Adwaita_Nerd_Font(const std::string& path);

    bool load() override;
    const std::vector<uint8_t>& getFontData() const override;
    void setFontData(const std::vector<uint8_t>& data) override;

    FontFormat getFormat() const override { return FontFormat::NOTO_BREEZE_ADWAITA_NERD; }
    bool save(const std::string& filepath) override;

    // Basic implementations - these will be enhanced as needed
    bool removeGlyph(const std::string& glyphName) override;
    bool removeGlyph(uint32_t unicode) override;
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override;

    // Information methods
    std::vector<GlyphInfo> listGlyphs() const override;
    std::vector<GlyphInfo> listGlyphMetadata() const override;
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override;
    std::string findGlyphName(uint32_t unicode) const override;
};

} // namespace fontmaster

#endif // NOTO_BREEZE_ADWAITA_NERD_FONT_H
