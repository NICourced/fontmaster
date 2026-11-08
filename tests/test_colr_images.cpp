#include "fontmaster/FontMaster.h"
#include "fontmaster/EmojiRenderer.h"
#include "utils/ImageCodec.h"
#include "utils/GlyphMetadata.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace fontmaster;
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected font and output directories");
        const std::filesystem::path fonts = argv[1], output = argv[2];
        auto bitmap = Font::load((fonts / "NotoColorEmoji.ttf").string());
        auto png = bitmap->getGlyphInfo(bitmap->findGlyphName(0x1F601)).image_data;
        auto font = Font::load((fonts / "Noto-COLRv1.ttf").string());
        const auto name = font->findGlyphName(0x1F600);
        const auto before = font->listGlyphs().size();
        check(font->replaceGlyphImage(name, png), "COLRv1 PNG replacement is not implemented");
        check(font->listGlyphs().size() == before, "replacement lost a glyph");
        check(font->getGlyphInfo(name).format == "svg", "PNG artwork must be stored in an SVG color presentation");
        check(!font->replaceGlyphImage(name, {1, 2, 3}), "invalid replacement accepted");
        const auto saved = (output / "colr-replaced.ttf").string();
        check(font->save(saved), "cannot save replacement");
        auto reloaded = Font::load(saved);
        check(reloaded->findGlyphName(0x1F600) == name, "replacement lookup lost after reload");
        check(reloaded->getGlyphInfo(name).image_data == font->getGlyphInfo(name).image_data, "replacement lost after reload");
        check(reloaded->removeGlyph(name), "cannot remove replacement");
        check(reloaded->findGlyphName(0x1F600).empty(), "removed replacement still visible");
        const std::vector<uint8_t> zeroColr{0,0, 0,1, 0,0,0,14, 0,0,0,20, 0,1,
            0,0, 0,0, 0,1, 0,1, 0,0};
        const std::vector<uint8_t> cpal{0,0, 0,1, 0,1, 0,1, 0,0,0,14, 0,0, 0,0,255,255};
        const auto zeroBytes = utils::rebuildSfnt(bitmap->getFontData(), {{"COLR", zeroColr}, {"CPAL", cpal}}, {"CBDT", "CBLC"});
        const auto zeroPath = (output / "colr-zero.ttf").string();
        check(detail::writeFontFile(zeroPath, zeroBytes), "cannot write glyph-zero fixture");
        auto zero = Font::load(zeroPath);
        check(zero->findGlyphName(0x10FFFF).empty(), "unmapped Unicode resolved to glyph zero");
        check(!zero->removeGlyph(uint32_t(0x10FFFF)), "unmapped Unicode removed glyph zero");
        auto invalidColr = zeroColr;
        const auto glyphCount = detail::GlyphMetadata(zeroBytes).count;
        invalidColr[14] = glyphCount >> 8; invalidColr[15] = glyphCount;
        const auto invalidBytes = utils::rebuildSfnt(zeroBytes, {{"COLR", invalidColr}});
        bool rejected = false;
        try { zero->setFontData(invalidBytes); } catch (const FontException&) { rejected = true; }
        check(rejected, "COLR base ID outside maxp accepted");
        check(zero->getFontData() == zeroBytes, "invalid COLR update mutated font");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
