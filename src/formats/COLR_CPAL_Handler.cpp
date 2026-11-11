#include "fontmaster/COLR_CPAL_Handler.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/MAXPParser.h"
#include "SVGTable.h"
#include "utils/GlyphMetadata.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <memory>
#include <set>

namespace fontmaster {

class COLR_CPAL_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, GlyphInfo> glyphs;
    std::vector<std::string> removedGlyphs;

    // Структуры для хранения данных COLR/CPAL
    struct ColorLayer {
        uint16_t glyphID;
        uint16_t paletteIndex;
    };

    struct BaseGlyph {
        uint16_t glyphID;
        std::vector<ColorLayer> layers;
    };

    struct Palette {
        std::vector<uint32_t> colors;
    };

    std::vector<BaseGlyph> baseGlyphs;
    std::vector<Palette> palettes;
    std::map<uint16_t, std::string> glyphNames;
    std::map<uint32_t, uint16_t> charToGlyph;
    std::map<uint16_t, uint32_t> glyphToChar;
    std::set<uint16_t> referencedBaseGlyphs;
    detail::SVGTable svg;
    uint16_t upem = 1000;
    uint16_t glyphCount = 0;

public:
    COLR_CPAL_Font(const std::string& path) : filepath(path) {
        loadFontData();
        parseFont();
    }

    bool load() override {
        // Уже загружено в конструкторе
        return true;
    }

    const std::vector<uint8_t>& getFontData() const override {
        return fontData;
    }

    void setFontData(const std::vector<uint8_t>& data) override {
        auto candidate = *this;
        candidate.fontData = data;
        candidate.parseFont();
        *this = std::move(candidate);
    }

    FontFormat getFormat() const override { return FontFormat::COLR_CPAL; }

    bool save(const std::string& outputPath) override {
        try {
            std::ofstream file(outputPath, std::ios::binary);
            if (!file) {
                throw FontSaveException(outputPath, "Cannot create output file");
            }

            file.write(reinterpret_cast<const char*>(fontData.data()), fontData.size());
            return file.good();
        } catch (const std::exception& e) {
            throw FontSaveException(outputPath, std::string("Save failed: ") + e.what());
        }
    }

    bool removeGlyph(const std::string& glyphName) override {
        const auto gid = findGlyphID(glyphName);
        const auto it = std::find_if(baseGlyphs.begin(), baseGlyphs.end(),
            [gid](const BaseGlyph& glyph) { return glyph.glyphID == gid; });
        if (it == baseGlyphs.end() && !svg.glyphs.count(gid)) return false;
        // Deleting a shared paint component would break other color glyphs.
        if (referencedBaseGlyphs.count(gid)) return false;
        const auto tables = utils::parseTTFTables(fontData);
        const auto* record = utils::findTable(tables, "COLR");
        std::vector<uint8_t> colr(fontData.begin() + record->offset,
                                  fontData.begin() + record->offset + record->length);
        auto put16 = [&](size_t offset, uint16_t value) {
            colr.at(offset) = value >> 8; colr.at(offset + 1) = value;
        };
        auto put32 = [&](size_t offset, uint32_t value) {
            for (int i = 0; i < 4; ++i) colr.at(offset + i) = value >> (24 - i * 8);
        };
        // Compact only the record array. Paints and layer arrays retain their
        // offsets and glyph IDs, including references from GSUB and outlines.
        auto filter = [&](size_t countOffset, size_t start, uint32_t count, bool wide) {
            size_t kept = 0;
            for (size_t i = 0; i < count; ++i) {
                const auto pos = start + i * 6;
                if (readUInt16(colr.data() + pos) == gid) continue;
                for (size_t j = 0; j < 6; ++j) colr[start + kept * 6 + j] = colr[pos + j];
                ++kept;
            }
            if (wide) put32(countOffset, kept); else put16(countOffset, kept);
        };
        filter(2, readUInt32(colr.data() + 4), readUInt16(colr.data() + 2), false);
        if (readUInt16(colr.data()) == 1) {
            const auto list = readUInt32(colr.data() + 14);
            if (list) filter(list, list + 4, readUInt32(colr.data() + list), true);
        }
        std::map<std::string, std::vector<uint8_t>> replacements{{"COLR", colr}};
        if (svg.glyphs.count(gid)) {
            auto edited = svg;
            edited.glyphs.erase(gid);
            replacements["SVG "] = edited.build();
        }
        setFontData(utils::rebuildSfnt(fontData, replacements));
        return true;
    }

    bool removeGlyph(uint32_t unicode) override {
        try {
            std::string glyphName = findGlyphName(unicode);
            if (glyphName.empty()) {
                std::cerr << "COLR_CPAL_Font: Glyph with Unicode U+" << std::hex << unicode << " not found" << std::endl;
                return false;
            }

            return removeGlyph(glyphName);

        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error removing glyph by Unicode: " << e.what() << std::endl;
            return false;
        }
    }

    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override {
        const auto gid = findGlyphID(glyphName);
        const bool hasColor = std::any_of(baseGlyphs.begin(), baseGlyphs.end(),
            [gid](const BaseGlyph& glyph) { return glyph.glyphID == gid; });
        if (!hasColor && !svg.glyphs.count(gid)) return false;
        auto replacement = svg;
        if (!replacement.replace(gid, newImage, upem)) return false;
        // A shared COLR component remains available to other paint graphs;
        // this glyph's direct presentation is selected from SVG by our renderer.
        auto candidate = *this;
        if (hasColor && !referencedBaseGlyphs.count(gid)) candidate.removeGlyph(glyphName);
        setFontData(utils::rebuildSfnt(candidate.fontData, {{"SVG ", replacement.build()}}));
        return true;
    }

private:
    std::vector<GlyphInfo> glyphList(bool includeImageData) const {
        std::set<uint16_t> ids;
        for (const auto& glyph : baseGlyphs) ids.insert(glyph.glyphID);
        for (const auto& entry : svg.glyphs) ids.insert(entry.first);
        std::vector<GlyphInfo> result;
        result.reserve(ids.size());
        for (const auto gid : ids) {
            const auto document = svg.glyphs.find(gid);
            if (document == svg.glyphs.end()) result.push_back({getGlyphName(gid), getUnicodeFromGlyphID(gid), {}, "colr", 0});
            else result.push_back({getGlyphName(gid), getUnicodeFromGlyphID(gid),
                includeImageData ? *document->second : std::vector<uint8_t>{}, "svg", document->second->size()});
        }
        return result;
    }

public:
    std::vector<GlyphInfo> listGlyphs() const override { return glyphList(true); }
    std::vector<GlyphInfo> listGlyphMetadata() const override { return glyphList(false); }

    GlyphInfo getGlyphInfo(const std::string& name) const override {
        const auto gid = findGlyphID(name);
        const auto document = svg.glyphs.find(gid);
        if (document != svg.glyphs.end())
            return {getGlyphName(gid), getUnicodeFromGlyphID(gid), *document->second, "svg", document->second->size()};
        const auto glyph = std::find_if(baseGlyphs.begin(), baseGlyphs.end(),
            [gid](const BaseGlyph& item) { return item.glyphID == gid; });
        if (glyph == baseGlyphs.end()) throw GlyphNotFoundException(name);
        return {getGlyphName(gid), getUnicodeFromGlyphID(gid), {}, "colr", 0};
    }

    std::string findGlyphName(uint32_t unicode) const override {
        try {
            const auto mapping = charToGlyph.find(unicode);
            if (mapping == charToGlyph.end()) return {};
            const auto glyphID = mapping->second;
            if (svg.glyphs.count(glyphID) || std::any_of(baseGlyphs.begin(), baseGlyphs.end(),
                [glyphID](const BaseGlyph& glyph) { return glyph.glyphID == glyphID; })) {
                return getGlyphName(glyphID);
            }

            return "";

        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error finding glyph name: " << e.what() << std::endl;
            return "";
        }
    }

private:
    void loadFontData() {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw FontLoadException(filepath, "Cannot open file");
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        fontData.resize(size);
        if (!file.read(reinterpret_cast<char*>(fontData.data()), size)) {
            throw FontLoadException(filepath, "Cannot read file data");
        }
    }

    void parseFont() {
        baseGlyphs.clear();
        palettes.clear();
        glyphNames.clear();
        removedGlyphs.clear();
        glyphs.clear();
        charToGlyph.clear();
        glyphToChar.clear();
        referencedBaseGlyphs.clear();
        const detail::GlyphMetadata metadata(fontData);
        upem = metadata.upem;
        glyphCount = metadata.count;
        const auto svgData = detail::tableBytes(fontData, "SVG ");
        svg = svgData.empty() ? detail::SVGTable{} : detail::SVGTable(svgData, metadata.count);
        auto tables = utils::parseTTFTables(fontData);
        const auto* cmap = utils::findTable(tables, "cmap");
        if (cmap) {
            std::vector<uint8_t> bytes(fontData.begin() + cmap->offset, fontData.begin() + cmap->offset + cmap->length);
            utils::CMAPParser mapping(bytes);
            if (!mapping.parse()) throw FontFormatException("COLR", "Invalid cmap table");
            charToGlyph = mapping.getCharToGlyphMap();
            for (const auto& pair : charToGlyph) glyphToChar.emplace(pair.second, pair.first);
        }
        if (!utils::hasTable(tables, "COLR") || !utils::hasTable(tables, "CPAL")) {
            throw FontFormatException("COLR/CPAL", "Required tables not found");
        }

        // Парсим COLR таблицу
        parseCOLRTable(tables);

        // Парсим CPAL таблицу
        parseCPALTable(tables);

        // Получаем имена глифов
        glyphNames = metadata.names;


    }

    void parseCOLRTable(const std::vector<utils::TableRecord>& tables) {
        const auto* rec = utils::findTable(tables, "COLR");
        if (!rec) return;
        const auto* data = fontData.data() + rec->offset;
        auto check = [&](size_t offset, size_t length) {
            if (offset > rec->length || length > rec->length - offset)
                throw FontFormatException("COLR", "Record outside table bounds");
        };
        check(0, 14);
        const auto version = readUInt16(data);
        if (version > 1) throw FontFormatException("COLR", "Unsupported table version");
        const auto count = readUInt16(data + 2);
        const size_t records = readUInt32(data + 4), layers = readUInt32(data + 8);
        const auto layerCount = readUInt16(data + 12);
        check(records, size_t(count) * 6);
        check(layers, size_t(layerCount) * 4);
        std::map<uint16_t, BaseGlyph> parsed;
        for (size_t i = 0; i < count; ++i) {
            const auto* record = data + records + i * 6;
            const auto first = readUInt16(record + 2), n = readUInt16(record + 4);
            if (size_t(first) + n > layerCount) throw FontFormatException("COLR", "Invalid layer range");
            BaseGlyph glyph{readUInt16(record), {}};
            if (glyph.glyphID >= glyphCount) throw FontFormatException("COLR", "Base glyph ID outside maxp");
            for (size_t j = 0; j < n; ++j) {
                const auto* layer = data + layers + (first + j) * 4;
                if (readUInt16(layer) >= glyphCount) throw FontFormatException("COLR", "Layer glyph ID outside maxp");
                glyph.layers.push_back({readUInt16(layer), readUInt16(layer + 2)});
            }
            parsed[glyph.glyphID] = std::move(glyph);
        }
        if (version == 1) {
            check(0, 34);
            const size_t list = readUInt32(data + 14);
            if (list) {
                std::set<uint16_t> paintGlyphIDs;
                check(list, 4);
                const size_t n = readUInt32(data + list);
                if (n > 65535) throw FontFormatException("COLR", "Too many base glyphs");
                check(list + 4, n * 6);
                for (size_t i = 0; i < n; ++i) {
                    const auto* record = data + list + 4 + i * 6;
                    const auto gid = readUInt16(record);
                    if (gid >= glyphCount || !paintGlyphIDs.insert(gid).second)
                        throw FontFormatException("COLR", "Invalid base paint glyph ID");
                    check(list + size_t(readUInt32(record + 2)), 1);
                    parsed[gid] = BaseGlyph{gid, {}};
                }
                // Follow the paint graph to protect shared PaintColrGlyph
                // components from removal. All offsets stay table-local.
                std::vector<size_t> pending;
                std::set<size_t> visited;
                for (size_t i = 0; i < n; ++i)
                    pending.push_back(list + readUInt32(data + list + 6 + i * 6));
                const size_t layerList = readUInt32(data + 18);
                auto u24 = [&](size_t offset) -> size_t {
                    check(offset, 3);
                    return (size_t(data[offset]) << 16) | (size_t(data[offset + 1]) << 8) | data[offset + 2];
                };
                while (!pending.empty()) {
                    const auto paint = pending.back(); pending.pop_back();
                    if (!visited.insert(paint).second) continue;
                    check(paint, 1);
                    const auto format = data[paint];
                    if (format == 1) {
                        check(paint, 6);
                        if (!layerList) throw FontFormatException("COLR", "Missing layer list");
                        check(layerList, 4);
                        const size_t first = readUInt32(data + paint + 2), count = data[paint + 1];
                        const size_t total = readUInt32(data + layerList);
                        if (first > total || count > total - first)
                            throw FontFormatException("COLR", "Invalid paint layer range");
                        check(layerList + 4, total * 4);
                        for (size_t j = 0; j < count; ++j)
                            pending.push_back(layerList + readUInt32(data + layerList + 4 + (first + j) * 4));
                    } else if (format == 11) {
                        check(paint, 3);
                        const auto gid = readUInt16(data + paint + 1);
                        if (!paintGlyphIDs.count(gid)) throw FontFormatException("COLR", "Missing referenced base paint glyph");
                        referencedBaseGlyphs.insert(gid);
                    } else if (format == 10 || (format >= 12 && format <= 31)) {
                        if (format == 10) {
                            check(paint, 6);
                            if (readUInt16(data + paint + 4) >= glyphCount)
                                throw FontFormatException("COLR", "Paint glyph ID outside maxp");
                        }
                        pending.push_back(paint + u24(paint + 1));
                    } else if (format == 32) {
                        pending.push_back(paint + u24(paint + 1));
                        pending.push_back(paint + u24(paint + 5));
                    } else if (format < 2 || format > 9) {
                        throw FontFormatException("COLR", "Unsupported paint format");
                    }
                }
            }
        }
        for (auto& entry : parsed) baseGlyphs.push_back(std::move(entry.second));
    }

    void parseCPALTable(const std::vector<utils::TableRecord>& tables) {
        const auto* rec = utils::findTable(tables, "CPAL");
        if (!rec) return;
        const auto* data = fontData.data() + rec->offset;
        auto check = [&](size_t offset, size_t length) {
            if (offset > rec->length || length > rec->length - offset)
                throw FontFormatException("CPAL", "Record outside table bounds");
        };
        check(0, 12);
        if (readUInt16(data) > 1) throw FontFormatException("CPAL", "Unsupported table version");
        const auto entries = readUInt16(data + 2), count = readUInt16(data + 4);
        const auto colors = readUInt16(data + 6);
        const size_t colorOffset = readUInt32(data + 8);
        check(12, size_t(count) * 2);
        check(colorOffset, size_t(colors) * 4);
        for (size_t i = 0; i < count; ++i) {
            const auto first = readUInt16(data + 12 + i * 2);
            if (size_t(first) + entries > colors) throw FontFormatException("CPAL", "Invalid palette range");
            Palette palette;
            for (size_t j = 0; j < entries; ++j)
                palette.colors.push_back(readUInt32(data + colorOffset + (first + j) * 4));
            palettes.push_back(std::move(palette));
        }
    }

    void parseGlyphNames(const std::vector<utils::TableRecord>& tables) {
        try {
            const utils::TableRecord* postRec = utils::findTable(tables, "post");
            const utils::TableRecord* maxpRec = utils::findTable(tables, "maxp");

            if (postRec && maxpRec) {
                utils::MAXPParser maxpParser(fontData, maxpRec->offset);
                if (maxpParser.parse()) {
                    uint16_t numGlyphs = maxpParser.getNumGlyphs();

                    utils::POSTParser postParser(fontData, postRec->offset, numGlyphs);
                    if (postParser.parse()) {
                        glyphNames = postParser.getGlyphNames();
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error parsing glyph names: " << e.what() << std::endl;
        }
    }

    // Вспомогательные методы
    uint16_t readUInt16(const uint8_t* data) const {
        // Make sure we have at least 2 bytes to read and data pointer is valid
        if (data == nullptr) return 0;
        // This is a simplified bounds check - in real implementation we'd need to pass buffer size
        return (static_cast<uint16_t>(data[0]) << 8) | data[1];
    }

    uint32_t readUInt32(const uint8_t* data) const {
        // Make sure we have at least 4 bytes to read and data pointer is valid
        if (data == nullptr) return 0;
        // This is a simplified bounds check - in real implementation we'd need to pass buffer size
        return (static_cast<uint32_t>(data[0]) << 24) |
               (static_cast<uint32_t>(data[1]) << 16) |
               (static_cast<uint32_t>(data[2]) << 8) |
               data[3];
    }

    std::string getGlyphName(uint16_t glyphID) const {
        auto it = glyphNames.find(glyphID);
        if (it != glyphNames.end() && !it->second.empty()) {
            return it->second;
        }

        std::ostringstream name;
        name << "glyph_" << glyphID;
        return name.str();
    }

    uint16_t findGlyphID(const std::string& glyphName) const {
        // Поиск по точному имени
        for (const auto& pair : glyphNames) {
            if (pair.second == glyphName) {
                return pair.first;
            }
        }

        if (glyphName.rfind("glyph_", 0) == 0) {
            const auto number = glyphName.substr(6);
            if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos) return UINT16_MAX;
            try {
                size_t consumed = 0;
                const auto value = std::stoul(number, &consumed);
                if (consumed == number.size() && value < UINT16_MAX) return static_cast<uint16_t>(value);
            } catch (const std::exception&) {}
        }
        return UINT16_MAX;
    }

    uint16_t findGlyphIDByUnicode(uint32_t unicode) const {
        const auto it = charToGlyph.find(unicode);
        return it == charToGlyph.end() ? 0 : it->second;
    }

    uint32_t getUnicodeFromGlyphID(uint16_t gid) const {
        const auto it = glyphToChar.find(gid);
        return it == glyphToChar.end() ? 0 : it->second;
    }


};

bool COLR_CPAL_Handler::canHandle(const std::string& filepath) {
    try {
        auto tables = utils::parseTTFTables(utils::readFontFile(filepath));
        return utils::hasTable(tables, "COLR") && utils::hasTable(tables, "CPAL");
    } catch (...) {
        return false;
    }
}

std::unique_ptr<Font> COLR_CPAL_Handler::loadFont(const std::string& filepath) {
    return std::unique_ptr<Font>(new COLR_CPAL_Font(filepath));
}

FontFormat COLR_CPAL_Handler::getFormat() const {
    return FontFormat::COLR_CPAL;
}

} // namespace fontmaster
