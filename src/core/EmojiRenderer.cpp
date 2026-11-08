#include "fontmaster/EmojiRenderer.h"
#include "fontmaster/FontMaster.h"
#include "../utils/GlyphMetadata.h"
#include "../utils/ImageCodec.h"

namespace fontmaster {
std::vector<uint8_t> renderGlyph(const Font& font, const std::string& glyphName, int size) {
    if (size < 1 || size > 4096) throw FontException("Render size must be between 1 and 4096 pixels");
    const auto glyph = font.getGlyphInfo(glyphName);
    const detail::GlyphMetadata metadata(font.getFontData());
    const auto gid = metadata.resolve(glyphName);
    if (!gid) throw GlyphNotFoundException(glyphName);
    if (glyph.format == "svg") return detail::renderSVG(glyph.image_data, *gid, size);
    if (glyph.format == "png" || glyph.format == "jpg" || glyph.format == "jpeg" || glyph.format == "tiff")
        return detail::renderRasterImage(glyph.image_data, size);
    return detail::renderFontGlyph(font.getFontData(), *gid, size);
}
namespace {
class FontRenderer final : public EmojiRenderer {
public:
    std::vector<uint8_t> renderGlyph(const std::string& path, const std::string& name, int size) override {
        auto font = Font::load(path);
        if (!font) throw FontLoadException(path, "No font handler accepted the file");
        return fontmaster::renderGlyph(*font, name, size);
    }
    std::vector<std::pair<std::string, uint32_t>> getGlyphs(const std::string& path) override {
        auto font = Font::load(path);
        if (!font) throw FontLoadException(path, "No font handler accepted the file");
        std::vector<std::pair<std::string, uint32_t>> result;
        for (const auto& glyph : font->listGlyphs()) result.emplace_back(glyph.name, glyph.unicode);
        return result;
    }
};
}
std::unique_ptr<EmojiRenderer> createEmojiRenderer() { return std::make_unique<FontRenderer>(); }
} // namespace fontmaster
