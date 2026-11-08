#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "utils/GlyphMetadata.h"
#include "utils/ImageCodec.h"
#include "formats/SVGTable.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace fontmaster;
using Bytes = std::vector<uint8_t>;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
static void put16(Bytes& data, size_t offset, uint16_t value) {
    data.at(offset) = value >> 8; data.at(offset + 1) = value;
}
static void put32(Bytes& data, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) data.at(offset + i) = value >> (24 - i * 8);
}
static Bytes bytes(const std::string& value) { return {value.begin(), value.end()}; }
static Bytes table(uint16_t first, uint16_t last, const Bytes& document) {
    Bytes result(24, 0);
    put32(result, 2, 10); put16(result, 10, 1);
    put16(result, 12, first); put16(result, 14, last);
    put32(result, 16, 14); put32(result, 20, document.size());
    result.insert(result.end(), document.begin(), document.end());
    return result;
}
static void rejectsTable(const Bytes& data, uint16_t count, const std::string& reason) {
    bool rejected = false;
    try { detail::SVGTable parsed(data, count); }
    catch (const std::exception&) { rejected = true; }
    require(rejected, reason);
}
static void tableTests() {
    const auto doc = bytes("<svg xmlns='http://www.w3.org/2000/svg'><g id='glyph1'><rect width='100' height='100'/></g></svg>");
    detail::SVGTable parsed(table(1, 3, doc), 5);
    require(parsed.glyphs.size() == 3, "SVG range did not include its endpoints");
    require(parsed.glyphs.at(1) == parsed.glyphs.at(3), "SVG shared document was duplicated");
    auto shifted = table(1, 1, doc);
    shifted.insert(shifted.begin() + 10, 10, 0);
    put32(shifted, 2, 20);
    require(*detail::SVGTable(shifted, 5).glyphs.at(1) == doc, "SVG offsets were not relative to the document list");
    parsed.glyphs.erase(2);
    auto split = parsed.build();
    require(split.size() == 36 + doc.size(), "Discontinuous ranges duplicated the SVG document");
    detail::SVGTable roundtrip(split, 5);
    require(roundtrip.glyphs.size() == 2 && !roundtrip.glyphs.count(2), "Split SVG ranges did not round-trip");
    require(roundtrip.glyphs.at(1) == roundtrip.glyphs.at(3) && *roundtrip.glyphs.at(3) == doc,
            "Shared SVG document changed after round-trip");
    const Bytes compressed{31,139,8,0,0,0,0,0,2,255,37,139,49,14,128,32,12,0,191,194,214,205,22,221,12,240,25,49,45,9,42,209,198,234,239,37,113,184,225,146,187,112,221,236,158,173,238,87,4,81,109,51,162,153,13,54,13,199,201,56,18,17,246,2,82,96,87,114,4,174,111,19,223,245,92,23,117,86,178,74,4,79,4,78,214,194,162,191,96,10,200,157,126,166,15,242,164,160,11,97,0,0,0};
    detail::SVGTable gzip(table(1, 1, compressed), 5);
    require(*gzip.glyphs.at(1) == compressed, "Gzip SVG bytes were not preserved");
    require(gzip.build() == table(1, 1, compressed), "Gzip SVG changed when rebuilt");
    require(!gzip.replace(1, bytes("<svg><broken>"), 1000), "Malformed SVG XML accepted");
    require(*gzip.glyphs.at(1) == compressed, "Invalid XML changed the SVG document");
    require(gzip.replace(1, bytes("<svg xmlns='http://www.w3.org/2000/svg' viewBox='17 23 100 200'><rect x='17' y='23' width='100' height='200' fill='red'/></svg>"), 1000), "Standalone SVG replacement failed");
    const std::string normalized(gzip.glyphs.at(1)->begin(), gzip.glyphs.at(1)->end());
    require(normalized.find("glyph1") != std::string::npos, "Standalone SVG was not assigned its OpenType glyph ID");
    require(normalized.find("17 23 100 200") != std::string::npos, "SVG normalization changed its viewBox");
    detail::SVGTable compressedReplacement(table(2, 2, doc), 5);
    require(compressedReplacement.replace(2, compressed, 1000), "Cannot replace with a compressed standalone SVG");
    const std::string normalizedGzip(compressedReplacement.glyphs.at(2)->begin(), compressedReplacement.glyphs.at(2)->end());
    require(normalizedGzip.find("glyph2") != std::string::npos, "Compressed replacement was not assigned its glyph ID");
    auto corrupt = table(1, 2, doc); put32(corrupt, 16, 0xFFFFFFFF);
    rejectsTable(corrupt, 5, "Out-of-table SVG offset accepted");
    corrupt = table(1, 2, doc); put32(corrupt, 20, 0xFFFFFFFF);
    rejectsTable(corrupt, 5, "Out-of-table SVG length accepted");
    corrupt = table(1, 2, doc); corrupt.resize(20);
    rejectsTable(corrupt, 5, "Truncated SVG record accepted");
    rejectsTable(table(3, 2, doc), 5, "Descending SVG range accepted");
    rejectsTable(table(1, 5, doc), 5, "Out-of-font SVG glyph accepted");
    rejectsTable(table(1, 2, bytes("not SVG")), 5, "Invalid SVG document accepted");
    corrupt = split; put16(corrupt, 24, 1);
    rejectsTable(corrupt, 5, "Overlapping SVG ranges accepted");
    detail::RasterImage red; red.width = 2; red.height = 1; red.rgba = {255,0,0,255,255,0,0,255};
    const auto png = detail::encodePNG(red);
    detail::SVGTable overlay;
    require(!overlay.replace(4, {1, 2, 3}, 1000) && overlay.glyphs.empty(), "Invalid overlay insertion changed the SVG table");
    require(overlay.replace(4, png, 1000) && overlay.glyphs.count(4), "Cannot add a new SVG overlay");
    require(parsed.replace(1, png, 1000), "PNG-to-SVG replacement failed");
    const auto rendered = detail::decodePNG(detail::renderSVG(*parsed.glyphs.at(1), 1, 32));
    require(rendered.width == 32 && rendered.height == 32, "Embedded PNG rendered at the wrong size");
    bool sawRed = false;
    for (size_t i = 0; i < rendered.rgba.size(); i += 4)
        if (rendered.rgba[i] > 200 && rendered.rgba[i + 1] < 20 && rendered.rgba[i + 3] > 200) sawRed = true;
    require(sawRed, "Embedded PNG did not render its pixels");
    const auto beforeInvalidPNG = parsed.glyphs.at(1);
    auto brokenPNG = png; brokenPNG.back() ^= 0xFF;
    require(!parsed.replace(1, brokenPNG, 1000), "PNG with a bad CRC accepted");
    require(parsed.glyphs.at(1) == beforeInvalidPNG, "Invalid PNG changed the SVG document");
    parsed.glyphs.clear();
    require(detail::SVGTable(parsed.build(), 5).glyphs.empty(), "Empty SVG edit cannot be reloaded");
}
// Retain real font tables, replacing color sources with a hand-built SVG table.
static Bytes svgFont(const Bytes& source, const Bytes& svg) {
    std::map<std::string, Bytes> tables;
    for (const auto& record : utils::parseTTFTables(source)) {
        const std::string tag(record.tag, 4);
        if (tag == "CBDT" || tag == "CBLC" || tag == "COLR" || tag == "CPAL" || tag == "sbix" || tag == "SVG ") continue;
        tables[tag] = Bytes(source.begin() + record.offset, source.begin() + record.offset + record.length);
    }
    tables["SVG "] = svg;
    Bytes result(source.begin(), source.begin() + 12);
    result.resize(12 + tables.size() * 16, 0);
    put16(result, 4, tables.size());
    uint16_t power = 1, selector = 0;
    while (size_t(power) * 2 <= tables.size()) { power *= 2; ++selector; }
    put16(result, 6, power * 16); put16(result, 8, selector);
    put16(result, 10, tables.size() * 16 - power * 16);
    size_t index = 0;
    for (const auto& entry : tables) {
        const size_t record = 12 + index++ * 16;
        std::copy(entry.first.begin(), entry.first.end(), result.begin() + record);
        put32(result, record + 8, result.size()); put32(result, record + 12, entry.second.size());
        result.insert(result.end(), entry.second.begin(), entry.second.end());
        while (result.size() % 4) result.push_back(0);
    }
    return utils::rebuildSfnt(result, {});
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected font fixture or directory and output directory");
        tableTests();
        auto fixture = std::filesystem::path(argv[1]);
        if (std::filesystem::is_directory(fixture)) fixture /= "NotoColorEmoji-flagsonly.ttf";
        const auto original = utils::readFontFile(fixture.string());
        const detail::GlyphMetadata metadata(original);
        require(metadata.count > 4, "Fixture needs at least five glyphs");
        const uint16_t first = 1, second = 2, third = 3;
        const auto document = bytes("<svg xmlns='http://www.w3.org/2000/svg'><g id='glyph1'><rect width='100' height='100' fill='red'/></g><g id='glyph2'><circle r='50' fill='blue'/></g><g id='glyph3'><rect width='30' height='30'/></g></svg>");
        const auto source = svgFont(original, table(first, third, document));
        const auto input = (std::filesystem::path(argv[2]) / "svg-source.ttf").string();
        require(detail::writeFontFile(input, source), "Cannot create SVG fixture");
        auto font = Font::load(input);
        require(font->getFormat() == FontFormat::SVG, "SVG handler is not registered");
        require(font->listGlyphs().size() == 3, "SVG glyph range was not parsed");
        const auto firstName = metadata.name(first), secondName = metadata.name(second);
        require(font->getGlyphInfo(firstName).image_data == document, "SVG document bytes changed");
        require(font->getGlyphInfo(firstName).unicode == metadata.unicode(first), "SVG Unicode metadata lost");
        const auto beforeInvalid = font->getFontData();
        require(!font->replaceGlyphImage(firstName, {1, 2, 3}), "Malformed SVG replacement accepted");
        require(font->getFontData() == beforeInvalid, "Failed replacement changed font");
        require(font->removeGlyph(secondName), "Cannot remove middle glyph of shared SVG range");
        require(!font->removeGlyph(secondName), "SVG glyph removed twice");
        require(font->getFontData() != source, "SVG removal did not update font bytes");
        require(font->getGlyphInfo(firstName).image_data == document, "SVG removal damaged sibling document");
        const auto replacement = bytes("<svg xmlns='http://www.w3.org/2000/svg' id='glyph1'><path d='M0 0h200v200z' fill='green'/></svg>");
        require(font->replaceGlyphImage(firstName, replacement), "SVG replacement failed");
        require(font->getGlyphInfo(firstName).image_data == replacement, "SVG replacement not exposed");
        const auto output = (std::filesystem::path(argv[2]) / "svg-edited.ttf").string();
        require(font->save(output), "Cannot save SVG edits");
        auto reloaded = Font::load(output);
        require(reloaded->listGlyphs().size() == 2, "SVG removal not persisted");
        require(reloaded->getGlyphInfo(firstName).image_data == replacement, "SVG replacement not persisted");
        font->setFontData(source);
        require(font->listGlyphs().size() == 3, "setFontData kept stale SVG cache");
        bool invalidRejected = false;
        try { font->setFontData({0, 0}); }
        catch (const std::runtime_error&) { invalidRejected = true; }
        require(invalidRejected, "Invalid font bytes accepted");
        require(font->getFontData() == source && font->listGlyphs().size() == 3, "Invalid setFontData was not atomic");
        require(!font->removeGlyph("glyph_65537") && !font->removeGlyph("glyph_1junk"), "Malformed SVG glyph ID accepted");
        const auto thirdName = metadata.name(third);
        if (metadata.codes.count(third)) {
            require(font->findGlyphName(metadata.unicode(third)) == thirdName, "SVG Unicode lookup failed");
            require(font->removeGlyph(metadata.unicode(third)), "Cannot remove SVG by Unicode");
            require(font->findGlyphName(metadata.unicode(third)).empty(), "Removed SVG still found by Unicode");
        }
        for (const auto& glyph : font->listGlyphs()) require(font->removeGlyph(glyph.name), "Cannot remove last SVG glyphs");
        const auto emptyOutput = (std::filesystem::path(argv[2]) / "svg-empty.ttf").string();
        require(font->save(emptyOutput), "Cannot save font with no SVG glyphs");
        require(Font::load(emptyOutput)->listGlyphs().empty(), "Empty SVG table did not round-trip");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
