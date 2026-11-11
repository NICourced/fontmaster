#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/POSTParser.h"
#include <fstream>
#include <limits>
#include <set>

namespace fontmaster {

CBDT_CBLC_Font::CBDT_CBLC_Font(const std::string& path) : filepath(path) {}

bool CBDT_CBLC_Font::ensureParsed() const {
    if (isParsed) return true;
    parser = std::make_unique<CBDT_CBLC_Parser>(fontData);
    if (!parser->parse()) { parser.reset(); return false; }
    cachedNames.clear();
    charToGlyph.clear();
    glyphToChar.clear();
    glyphCount = 0;
    const auto tables = utils::parseTTFTables(fontData);
    const auto* maxp = utils::findTable(tables, "maxp");
    if (maxp && maxp->length >= 6)
        glyphCount = (uint16_t(fontData[maxp->offset + 4]) << 8) | fontData[maxp->offset + 5];
    const auto* post = utils::findTable(tables, "post");
    if (post && post->length >= 32) {
        // Restrict the POST parser to its own table.
        std::vector<uint8_t> bytes(fontData.begin() + post->offset, fontData.begin() + post->offset + post->length);
        utils::POSTParser names(bytes, 0, glyphCount);
        if (names.parse()) cachedNames = names.getGlyphNames();
    }
    const auto* cmap = utils::findTable(tables, "cmap");
    if (cmap) {
        std::vector<uint8_t> bytes(fontData.begin() + cmap->offset, fontData.begin() + cmap->offset + cmap->length);
        utils::CMAPParser mapping(bytes);
        if (!mapping.parse()) throw FontFormatException("CBDT/CBLC", "Invalid cmap table");
        charToGlyph = mapping.getCharToGlyphMap();
        for (const auto& pair : charToGlyph) glyphToChar.emplace(pair.second, pair.first);
    }
    isParsed = true;
    return true;
}

bool CBDT_CBLC_Font::load() {
    if (fontData.empty()) setFontData(utils::readFontFile(filepath));
    return ensureParsed();
}

bool CBDT_CBLC_Font::save(const std::string& outputPath) {
    if (!ensureParsed()) return false;
    CBDT_CBLC_Rebuilder rebuilder(fontData, parser->getStrikes(), parser->getRemovedGlyphs());
    const auto bytes = rebuilder.rebuild();
    std::ofstream file(outputPath, std::ios::binary);
    return file && file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()).good();
}

const std::vector<uint8_t>& CBDT_CBLC_Font::getFontData() const { return fontData; }
void CBDT_CBLC_Font::setFontData(const std::vector<uint8_t>& data) {
    fontData = data;
    parser.reset();
    isParsed = false;
    cachedNames.clear();
    charToGlyph.clear();
    glyphToChar.clear();
    glyphCount = 0;
}

const std::map<uint16_t, StrikeRecord>& CBDT_CBLC_Font::getStrikes() const {
    static const std::map<uint16_t, StrikeRecord> empty;
    return ensureParsed() ? parser->getStrikes() : empty;
}
const std::vector<uint16_t>& CBDT_CBLC_Font::getRemovedGlyphs() const {
    static const std::vector<uint16_t> empty;
    return ensureParsed() ? parser->getRemovedGlyphs() : empty;
}

bool CBDT_CBLC_Font::removeGlyph(const std::string& name) {
    return ensureParsed() && parser->removeGlyph(findGlyphID(name));
}
bool CBDT_CBLC_Font::removeGlyph(uint32_t unicode) {
    const auto name = findGlyphName(unicode);
    return !name.empty() && removeGlyph(name);
}
bool CBDT_CBLC_Font::replaceGlyphImage(const std::string& name, const std::vector<uint8_t>& png) {
    return ensureParsed() && parser->replaceGlyphImage(findGlyphID(name), png);
}

std::vector<GlyphInfo> CBDT_CBLC_Font::listGlyphs() const { return glyphList(true); }
std::vector<GlyphInfo> CBDT_CBLC_Font::listGlyphMetadata() const { return glyphList(false); }

std::vector<GlyphInfo> CBDT_CBLC_Font::glyphList(bool includeImageData) const {
    std::vector<GlyphInfo> result;
    if (!ensureParsed()) return result;
    std::set<uint16_t> seen;
    for (const auto& strike : parser->getStrikes()) {
        for (const auto& entry : strike.second.glyphImages) {
            const auto gid = entry.first;
            if (!seen.insert(gid).second) continue;
            const auto& image = entry.second;
            GlyphInfo info{};
            info.name = getGlyphName(gid, cachedNames);
            info.unicode = getUnicodeFromGlyphID(gid);
            if (includeImageData) info.image_data = image.data;
            info.format = getImageFormatString(image.imageFormat);
            info.data_size = image.data.size();
            result.push_back(std::move(info));
        }
    }
    return result;
}

GlyphInfo CBDT_CBLC_Font::getGlyphInfo(const std::string& name) const {
    if (!ensureParsed()) throw GlyphNotFoundException(name);
    const auto gid = findGlyphID(name);
    for (const auto& strike : parser->getStrikes()) {
        const auto it = strike.second.glyphImages.find(gid);
        if (it == strike.second.glyphImages.end()) continue;
        const auto& image = it->second;
        return {getGlyphName(gid, cachedNames), getUnicodeFromGlyphID(gid), image.data,
                getImageFormatString(image.imageFormat), image.data.size()};
    }
    throw GlyphNotFoundException(name);
}

std::string CBDT_CBLC_Font::findGlyphName(uint32_t unicode) const {
    if (!ensureParsed()) return {};
    const auto gid = findGlyphIDByUnicode(unicode);
    if (!gid) return {};
    for (const auto& strike : parser->getStrikes())
        if (strike.second.glyphImages.count(gid)) return getGlyphName(gid, cachedNames);
    return {};
}

std::string CBDT_CBLC_Font::getGlyphName(uint16_t gid, const std::map<uint16_t, std::string>& names) const {
    const auto it = names.find(gid);
    return it == names.end() ? "glyph_" + std::to_string(gid) : it->second;
}
std::map<uint16_t, std::string> CBDT_CBLC_Font::getPostGlyphNames() const { ensureParsed(); return cachedNames; }
uint16_t CBDT_CBLC_Font::getMaxpGlyphCount() const { ensureParsed(); return glyphCount; }

uint16_t CBDT_CBLC_Font::findGlyphID(const std::string& name) const {
    for (const auto& entry : cachedNames) if (entry.second == name) return entry.first;
    try {
        if (name.rfind("glyph_", 0) == 0) {
            const auto number = name.substr(6);
            size_t consumed = 0;
            const auto value = std::stoul(number, &consumed);
            if (consumed == number.size() && value < glyphCount) return value;
        } else if (name.rfind("u", 0) == 0) {
            const size_t prefix = name.rfind("uni", 0) == 0 ? 3 : 1;
            const auto number = name.substr(prefix);
            size_t consumed = 0;
            const auto value = std::stoul(number, &consumed, 16);
            if (consumed == number.size() && value <= 0x10FFFF) return findGlyphIDByUnicode(value);
        }
    } catch (const std::exception&) {}
    return std::numeric_limits<uint16_t>::max();
}
uint16_t CBDT_CBLC_Font::findGlyphIDByUnicode(uint32_t unicode) const {
    const auto it = charToGlyph.find(unicode);
    return it == charToGlyph.end() ? 0 : it->second;
}
uint32_t CBDT_CBLC_Font::getUnicodeFromGlyphID(uint16_t gid) const {
    const auto it = glyphToChar.find(gid);
    return it == glyphToChar.end() ? 0 : it->second;
}
std::string CBDT_CBLC_Font::getImageFormatString(uint16_t format) const {
    return format >= 17 && format <= 19 ? "png" : "bitmap";
}
} // namespace fontmaster
