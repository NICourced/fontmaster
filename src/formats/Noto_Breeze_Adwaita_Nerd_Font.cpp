#include "fontmaster/Noto_Breeze_Adwaita_Nerd_Font.h"
#include "StandardFont.h"
namespace fontmaster {
Noto_Breeze_Adwaita_Nerd_Font::Noto_Breeze_Adwaita_Nerd_Font(const std::string& path)
    : filepath(path), implementation(detail::createStandardFont(path, FontFormat::NOTO_BREEZE_ADWAITA_NERD)) {}
bool Noto_Breeze_Adwaita_Nerd_Font::load() { return implementation->load(); }
const std::vector<uint8_t>& Noto_Breeze_Adwaita_Nerd_Font::getFontData() const { return implementation->getFontData(); }
void Noto_Breeze_Adwaita_Nerd_Font::setFontData(const std::vector<uint8_t>& data) { implementation->setFontData(data); }
bool Noto_Breeze_Adwaita_Nerd_Font::save(const std::string& path) { return implementation->save(path); }
bool Noto_Breeze_Adwaita_Nerd_Font::removeGlyph(const std::string& name) { return implementation->removeGlyph(name); }
bool Noto_Breeze_Adwaita_Nerd_Font::removeGlyph(uint32_t unicode) { return implementation->removeGlyph(unicode); }
bool Noto_Breeze_Adwaita_Nerd_Font::replaceGlyphImage(const std::string& name, const std::vector<uint8_t>& image) { return implementation->replaceGlyphImage(name, image); }
std::vector<GlyphInfo> Noto_Breeze_Adwaita_Nerd_Font::listGlyphs() const { return implementation->listGlyphs(); }
std::vector<GlyphInfo> Noto_Breeze_Adwaita_Nerd_Font::listGlyphMetadata() const { return implementation->listGlyphMetadata(); }
GlyphInfo Noto_Breeze_Adwaita_Nerd_Font::getGlyphInfo(const std::string& name) const { return implementation->getGlyphInfo(name); }
std::string Noto_Breeze_Adwaita_Nerd_Font::findGlyphName(uint32_t unicode) const { return implementation->findGlyphName(unicode); }
} // namespace fontmaster
