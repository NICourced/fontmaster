#include "SVGOverlayFont.h"
#include "SVGTable.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include "utils/GlyphMetadata.h"
#include <set>

namespace fontmaster::detail {
namespace {
using Bytes = std::vector<uint8_t>;
Bytes materialize(Font& primary) {
    if (auto* cbdt = dynamic_cast<CBDT_CBLC_Font*>(&primary)) {
        CBDT_CBLC_Rebuilder builder(cbdt->getFontData(), cbdt->getStrikes(), cbdt->getRemovedGlyphs());
        return builder.rebuild();
    }
    return primary.getFontData();
}
struct State {
    Bytes data;
    GlyphMetadata metadata;
    SVGTable svg;
    std::map<uint16_t, std::string> bitmapNames;
    explicit State(const Bytes& bytes) : data(bytes), metadata(bytes) {
        const auto tables = utils::parseTTFTables(bytes);
        if (utils::hasTable(tables, "SVG ")) svg = SVGTable(tableBytes(bytes, "SVG "), metadata.count);
    }
    void mapBitmaps(const Font& primary) {
        for (const auto& glyph : primary.listGlyphs()) {
            auto gid = metadata.resolve(glyph.name);
            if (!gid) {
                const auto unicode = metadata.chars.find(glyph.unicode);
                if (unicode != metadata.chars.end()) gid = unicode->second;
            }
            if (!gid) throw FontFormatException("SVG/bitmap", "Cannot resolve bitmap glyph '" + glyph.name + "'");
            bitmapNames.emplace(*gid, glyph.name);
        }
    }
    bool contains(uint16_t gid) const { return svg.glyphs.count(gid) || bitmapNames.count(gid); }
};

class SVGOverlayFont final : public Font {
    std::unique_ptr<Font> primary;
    std::unique_ptr<State> state;

    // Build the complete candidate before publishing any SVG or byte changes.
    // The caller restores its bitmap snapshot if a primary edit fails later.
    void apply(const SVGTable& svg) {
        auto bytes = utils::rebuildSfnt(materialize(*primary), {{"SVG ", svg.build()}});
        auto candidate = std::make_unique<State>(bytes);
        candidate->mapBitmaps(*primary);
        state = std::move(candidate);
    }
    GlyphInfo info(uint16_t gid) const {
        GlyphInfo result{};
        const auto document = state->svg.glyphs.find(gid);
        if (document != state->svg.glyphs.end()) {
            result.image_data = *document->second;
            result.data_size = result.image_data.size();
            result.format = "svg";
        } else {
            const auto bitmap = state->bitmapNames.find(gid);
            if (bitmap == state->bitmapNames.end()) throw GlyphNotFoundException(state->metadata.name(gid));
            result = primary->getGlyphInfo(bitmap->second);
        }
        result.name = state->metadata.name(gid);
        result.unicode = state->metadata.unicode(gid);
        return result;
    }
public:
    explicit SVGOverlayFont(std::unique_ptr<Font> font) : primary(std::move(font)) {
        auto candidate = std::make_unique<State>(materialize(*primary));
        candidate->mapBitmaps(*primary);
        state = std::move(candidate);
    }
    FontFormat getFormat() const override { return primary->getFormat(); }
    const Bytes& getFontData() const override { return state->data; }
    void setFontData(const Bytes& bytes) override {
        auto candidate = std::make_unique<State>(bytes);
        try {
            primary->setFontData(bytes);
            // CBDT parsing is lazy; force validation before committing the new state.
            if (auto* cbdt = dynamic_cast<CBDT_CBLC_Font*>(primary.get()))
                if (!cbdt->load()) throw FontFormatException("SVG/bitmap", "Invalid CBDT/CBLC tables");
            candidate->mapBitmaps(*primary);
        } catch (...) {
            primary->setFontData(state->data);
            throw;
        }
        state = std::move(candidate);
    }
    bool load() override {
        try {
            if (!primary->load()) { primary->setFontData(state->data); return false; }
            auto candidate = std::make_unique<State>(materialize(*primary));
            candidate->mapBitmaps(*primary);
            state = std::move(candidate);
            return true;
        } catch (const std::exception&) {
            primary->setFontData(state->data);
            return false;
        }
    }
    bool save(const std::string& path) override {
        if (!writeFontFile(path, state->data)) throw FontSaveException(path, "Cannot write font data");
        return true;
    }
    std::vector<GlyphInfo> listGlyphs() const override {
        std::set<uint16_t> ids;
        for (const auto& glyph : state->bitmapNames) ids.insert(glyph.first);
        for (const auto& glyph : state->svg.glyphs) ids.insert(glyph.first);
        std::vector<GlyphInfo> result;
        result.reserve(ids.size());
        for (const auto gid : ids) result.push_back(info(gid));
        return result;
    }
    GlyphInfo getGlyphInfo(const std::string& name) const override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->contains(*gid)) throw GlyphNotFoundException(name);
        return info(*gid);
    }
    std::string findGlyphName(uint32_t unicode) const override {
        const auto found = state->metadata.chars.find(unicode);
        return found != state->metadata.chars.end() && state->contains(found->second)
            ? state->metadata.name(found->second) : "";
    }
    bool removeGlyph(uint32_t unicode) override {
        const auto name = findGlyphName(unicode);
        return !name.empty() && removeGlyph(name);
    }
    bool removeGlyph(const std::string& name) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->contains(*gid)) return false;
        auto svg = state->svg;
        svg.glyphs.erase(*gid);
        const auto bitmap = state->bitmapNames.find(*gid);
        const bool editsPrimary = bitmap != state->bitmapNames.end();
        try {
            if (editsPrimary && !primary->removeGlyph(bitmap->second)) {
                primary->setFontData(state->data);
                return false;
            }
            apply(svg);
            return true;
        } catch (const std::exception&) {
            if (editsPrimary) primary->setFontData(state->data);
            return false;
        }
    }
    bool replaceGlyphImage(const std::string& name, const Bytes& image) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->contains(*gid)) return false;
        auto svg = state->svg;
        if (svg.glyphs.count(*gid)) {
            if (!svg.replace(*gid, image, state->metadata.upem)) return false;
            try { apply(svg); return true; }
            catch (const std::exception&) { return false; }
        }
        try {
            if (!primary->replaceGlyphImage(state->bitmapNames.at(*gid), image)) {
                primary->setFontData(state->data);
                return false;
            }
            apply(svg);
            return true;
        } catch (const std::exception&) {
            primary->setFontData(state->data);
            return false;
        }
    }
};
} // namespace

std::unique_ptr<Font> withSVGPresentations(std::unique_ptr<Font> primary) {
    if (!primary || dynamic_cast<SVGOverlayFont*>(primary.get())) return primary;
    if (primary->getFormat() != FontFormat::CBDT_CBLC && primary->getFormat() != FontFormat::SBIX) return primary;
    if (!utils::hasTable(utils::parseTTFTables(primary->getFontData()), "SVG ")) return primary;
    return std::make_unique<SVGOverlayFont>(std::move(primary));
}
} // namespace fontmaster::detail
