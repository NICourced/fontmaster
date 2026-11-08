#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/EmojiRenderer.h"
#include "fontmaster/TTFUtils.h"
#include "formats/SVGTable.h"
#include "utils/GlyphMetadata.h"
#include "utils/ImageCodec.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace fontmaster;
using Bytes = std::vector<uint8_t>;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void put16(Bytes& bytes, size_t p, uint16_t value) {
    bytes.at(p) = value >> 8; bytes.at(p + 1) = value;
}
static void put32(Bytes& bytes, size_t p, uint32_t value) {
    for (unsigned n = 0; n < 4; ++n) bytes.at(p + n) = value >> (24 - 8 * n);
}
static Bytes svgDocument(uint16_t first, uint16_t second) {
    const auto value = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><g id='glyph" +
        std::to_string(first) + "'><rect width='100' height='100' fill='red'/></g><g id='glyph" +
        std::to_string(second) + "'><rect width='100' height='100' fill='blue'/></g></svg>";
    return {value.begin(), value.end()};
}
static Bytes withSVG(const Bytes& bytes, uint16_t first, uint16_t second) {
    detail::SVGTable svg;
    const auto document = std::make_shared<const Bytes>(svgDocument(first, second));
    svg.glyphs[first] = document; svg.glyphs[second] = document;
    return utils::rebuildSfnt(bytes, {{"SVG ", svg.build()}});
}
static Bytes solidPNG(uint8_t red, uint8_t green, uint8_t blue) {
    return detail::encodePNG({1, 1, {red, green, blue, 255}});
}
static void renders(const Font& font, const std::string& name, uint8_t red, uint8_t green, uint8_t blue) {
    const auto decoded = detail::decodePNG(renderGlyph(font, name, 24));
    const size_t center = (12 * 24 + 12) * 4;
    require(decoded.width == 24 && decoded.height == 24 && decoded.rgba.at(center) == red &&
        decoded.rgba.at(center + 1) == green && decoded.rgba.at(center + 2) == blue &&
        decoded.rgba.at(center + 3) == 255, "Mixed glyph renderer did not use its preferred presentation");
}
static Bytes simpleSBIX(uint16_t count, uint16_t first, uint16_t second, const Bytes& image) {
    Bytes table(12 + 4 + (size_t(count) + 1) * 4);
    put16(table, 0, 1); put16(table, 2, 1); put32(table, 4, 1); put32(table, 8, 12);
    put16(table, 12, 32); put16(table, 14, 72);
    for (uint32_t gid = 0; gid < count; ++gid) {
        put32(table, 16 + gid * 4, table.size() - 12);
        if (gid == first || gid == second) {
            table.insert(table.end(), {0,0,0,0,'p','n','g',' '});
            table.insert(table.end(), image.begin(), image.end());
        }
    }
    put32(table, 16 + size_t(count) * 4, table.size() - 12);
    return table;
}
static void mixedCase(const Bytes& source, const std::filesystem::path& dir, const std::string& stem,
                      FontFormat format, uint16_t svgOnly, uint16_t both, uint16_t bitmapOnly) {
    const detail::GlyphMetadata metadata(source);
    const auto path = (dir / (stem + ".ttf")).string();
    require(detail::writeFontFile(path, source), "Cannot write mixed font fixture");
    auto font = Font::load(path);
    require(font->getFormat() == format, "Mixed font lost its primary format");
    const auto svgName = metadata.name(svgOnly), bothName = metadata.name(both), bitmapName = metadata.name(bitmapOnly);
    require(font->getGlyphInfo(svgName).format == "svg", "SVG-only glyph is hidden behind bitmap table precedence");
    require(font->getGlyphInfo(bothName).format == "svg", "Bitmap presentation hides the preferred SVG");
    require(font->getGlyphInfo(bitmapName).format == "png", "Distinct bitmap presentation is hidden");
    const auto originalGlyphs = font->listGlyphs();
    std::set<std::string> names;
    for (const auto& glyph : originalGlyphs) names.insert(glyph.name);
    require(names.size() == originalGlyphs.size(), "Shared SVG/bitmap glyph listed twice");
    require(names.count(svgName) && names.count(bothName) && names.count(bitmapName), "Mixed glyph union omitted a presentation");
    renders(*font, svgName, 255, 0, 0); renders(*font, bothName, 0, 0, 255);
    const auto untouchedDocument = font->getGlyphInfo(svgName).image_data;
    if (metadata.codes.count(svgOnly))
        require(font->findGlyphName(metadata.unicode(svgOnly)) == svgName, "SVG Unicode lookup lost in mixed font");
    const auto beforeInvalid = font->getFontData();
    require(!font->replaceGlyphImage(bothName, {1,2,3}), "Malformed mixed SVG replacement accepted");
    require(!font->replaceGlyphImage(bitmapName, {1,2,3}), "Malformed mixed bitmap replacement accepted");
    require(!font->removeGlyph("glyph_65536") && !font->removeGlyph("glyph_1junk"), "Malformed mixed glyph ID accepted");
    require(font->getFontData() == beforeInvalid, "Failed mixed edit changed the font bytes");
    const auto green = solidPNG(0,255,0), yellow = solidPNG(255,255,0);
    require(font->replaceGlyphImage(bothName, green), "Cannot replace mixed SVG presentation with PNG");
    require(font->getGlyphInfo(bothName).format == "svg", "SVG replacement switched to the hidden bitmap");
    renders(*font, bothName, 0,255,0);
    require(font->replaceGlyphImage(bitmapName, yellow), "Cannot replace a distinct mixed bitmap");
    require(font->getGlyphInfo(bitmapName).image_data == yellow, "Mixed bitmap edit was not exposed");
    require(font->getGlyphInfo(svgName).image_data == untouchedDocument, "Bitmap edit changed an untouched SVG document");
    renders(*font, bitmapName, 255,255,0);
    const auto editedBytes = font->getFontData();
    require(editedBytes != beforeInvalid, "Mixed getFontData ignored edits");
    require(!font->replaceGlyphImage(bitmapName, {1,2,3}) && font->getFontData() == editedBytes,
            "Failed bitmap edit lost an earlier mixed SVG edit");
    renders(*font, bothName, 0,255,0);
    const auto saved = (dir / (stem + "-edited.ttf")).string();
    require(font->save(saved), "Cannot save mixed edits");
    auto reloaded = Font::load(saved);
    require(reloaded->getGlyphInfo(bitmapName).image_data == yellow, "Bitmap edit lost during mixed save");
    renders(*reloaded, bothName, 0,255,0);
    require(reloaded->removeGlyph(bothName), "Cannot remove both mixed presentations");
    require(!reloaded->removeGlyph(bothName), "Hidden bitmap reappeared after SVG removal");
    require(reloaded->listGlyphs().size() == originalGlyphs.size() - 1, "Mixed removal omitted or duplicated a glyph");
    const auto bitmapData = utils::rebuildSfnt(reloaded->getFontData(), {}, {"SVG "});
    const auto primaryPath = (dir / (stem + "-primary.ttf")).string();
    require(detail::writeFontFile(primaryPath, bitmapData), "Cannot create independent primary check");
    auto primary = Font::load(primaryPath);
    bool hiddenRemoved = false;
    try { primary->getGlyphInfo(bothName); } catch (const GlyphNotFoundException&) { hiddenRemoved = true; }
    require(hiddenRemoved, "Removing SVG left its hidden bitmap presentation in the file");
    require(reloaded->save(saved), "Cannot save mixed removal");
    require(Font::load(saved)->listGlyphs().size() == originalGlyphs.size() - 1, "Mixed removal was not persistent");
    reloaded->setFontData(source);
    require(reloaded->listGlyphs().size() == originalGlyphs.size(), "Mixed setFontData retained stale removals");
    renders(*reloaded, bothName, 0,0,255);
    const auto retained = reloaded->getFontData();
    auto badSVG = detail::tableBytes(source, "SVG "); badSVG.at(2) = 255;
    bool rejected = false;
    try { reloaded->setFontData(utils::rebuildSfnt(source, {{"SVG ", badSVG}})); }
    catch (const std::exception&) { rejected = true; }
    require(rejected && reloaded->getFontData() == retained, "Mixed setFontData was not transactional");
    rejected = false;
    const auto primaryTag = format == FontFormat::CBDT_CBLC ? "CBLC" : "sbix";
    try { reloaded->setFontData(utils::rebuildSfnt(source, {{primaryTag, {0,0}}})); }
    catch (const std::exception&) { rejected = true; }
    require(rejected && reloaded->getFontData() == retained, "Invalid primary table lost mixed state during rollback");
    renders(*reloaded, bothName, 0,0,255);
    require(reloaded->removeGlyph(svgName), "Cannot remove SVG-only mixed glyph");
    require(reloaded->getGlyphInfo(bitmapName).format == "png", "SVG-only removal damaged bitmap sibling");
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected fixture directory and output directory");
        const std::filesystem::path fixtures(argv[1]), output(argv[2]);
        std::filesystem::create_directories(output);
        auto bitmap = Font::load((fixtures / "NotoColorEmoji-flagsonly.ttf").string());
        const auto original = bitmap->getFontData();
        const detail::GlyphMetadata metadata(original);
        std::set<uint16_t> present;
        for (const auto& glyph : bitmap->listGlyphs()) {
            const auto gid = metadata.resolve(glyph.name);
            require(bool(gid), "Cannot resolve fixture glyph"); present.insert(*gid);
        }
        require(present.size() > 2, "Fixture needs bitmap glyphs");
        const auto both = *present.begin(), bitmapOnly = *std::next(present.begin());
        uint16_t svgOnly = metadata.count;
        for (const auto& entry : metadata.chars) if (!present.count(entry.second)) { svgOnly = entry.second; break; }
        if (svgOnly == metadata.count)
            for (uint32_t gid = 0; gid < metadata.count; ++gid) if (!present.count(gid)) { svgOnly = gid; break; }
        require(svgOnly < metadata.count, "Fixture needs a glyph without a bitmap");
        mixedCase(withSVG(original, svgOnly, both), output, "mixed-cbdt", FontFormat::CBDT_CBLC, svgOnly, both, bitmapOnly);
        auto* cbdt = dynamic_cast<CBDT_CBLC_Font*>(bitmap.get());
        require(cbdt, "Fixture is not a CBDT font");
        auto strikes = cbdt->getStrikes();
        auto& images = strikes.begin()->second.glyphImages;
        require(images.size() > 1, "Fixture needs two images in one strike");
        const auto component = images.begin()->first, composite = std::next(images.begin())->first;
        auto& compositeImage = images.at(composite);
        compositeImage.imageFormat = 8;
        compositeImage.recordData.assign(5, 0);
        compositeImage.recordData.insert(compositeImage.recordData.end(), {0,0,1,
            static_cast<uint8_t>(component >> 8), static_cast<uint8_t>(component), 0,0});
        CBDT_CBLC_Rebuilder builder(original, strikes, {});
        const auto dependencyPath = (output / "mixed-cbdt-dependency.ttf").string();
        require(detail::writeFontFile(dependencyPath, withSVG(builder.rebuild(), svgOnly, component)), "Cannot write mixed dependency fixture");
        auto dependent = Font::load(dependencyPath);
        const auto retained = dependent->getFontData();
        require(!dependent->removeGlyph(metadata.name(component)), "Mixed SVG removal bypassed a bitmap composite dependency");
        require(dependent->getFontData() == retained && dependent->getGlyphInfo(metadata.name(component)).format == "svg",
                "Blocked bitmap removal lost its SVG presentation");
        require(dependent->removeGlyph(metadata.name(composite)), "Cannot remove a mixed bitmap composite");
        require(dependent->removeGlyph(metadata.name(component)), "Unused mixed bitmap component remains protected");
        const auto sbix = utils::rebuildSfnt(original,
            {{"sbix", simpleSBIX(metadata.count, both, bitmapOnly, solidPNG(255,0,255))}}, {"CBDT", "CBLC"});
        mixedCase(withSVG(sbix, svgOnly, both), output, "mixed-sbix", FontFormat::SBIX, svgOnly, both, bitmapOnly);
        std::cout << "Mixed SVG/bitmap tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
