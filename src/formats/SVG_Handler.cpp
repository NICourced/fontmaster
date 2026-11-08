#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "SVGTable.h"
#include "utils/GlyphMetadata.h"
#include "StandardFont.h"

namespace fontmaster {
namespace {
std::set<uint16_t> visibleOutlines(const std::vector<uint8_t>& data) {
    const auto tables = utils::parseTTFTables(data);
    if (!utils::hasTable(tables, "glyf") && !utils::hasTable(tables, "CFF ") && !utils::hasTable(tables, "CFF2")) return {};
    return detail::outlineGlyphs(data);
}
class SVGFont final : public Font {
    struct State {
        std::vector<uint8_t> data;
        detail::GlyphMetadata metadata;
        detail::SVGTable svg;
        std::set<uint16_t> outlines;
        explicit State(const std::vector<uint8_t>& bytes)
            : data(bytes), metadata(bytes), svg(detail::tableBytes(bytes, "SVG "), metadata.count),
              outlines(visibleOutlines(bytes)) {}
    };
    std::string filepath;
    std::unique_ptr<State> state;

    GlyphInfo info(uint16_t gid) const {
        GlyphInfo result{};
        result.name = state->metadata.name(gid);
        result.unicode = state->metadata.unicode(gid);
        const auto document = state->svg.glyphs.find(gid);
        if (document == state->svg.glyphs.end()) result.format = "outline";
        else {
            result.image_data = *document->second;
            result.data_size = result.image_data.size();
            result.format = "svg";
        }
        return result;
    }
    void apply(detail::SVGTable svg) {
        auto rebuilt = utils::rebuildSfnt(state->data, {{"SVG ", svg.build()}});
        state->data = std::move(rebuilt);
        state->svg = std::move(svg);
    }
public:
    explicit SVGFont(const std::string& path) : filepath(path) { setFontData(utils::readFontFile(path)); }
    FontFormat getFormat() const override { return FontFormat::SVG; }
    bool load() override {
        try { setFontData(utils::readFontFile(filepath)); return true; }
        catch (const std::exception&) { return false; }
    }
    const std::vector<uint8_t>& getFontData() const override { return state->data; }
    void setFontData(const std::vector<uint8_t>& data) override {
        auto replacement = std::make_unique<State>(data);
        state = std::move(replacement);
    }
    bool removeGlyph(const std::string& name) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid) return false;
        if (!state->svg.glyphs.count(*gid)) {
            if (!state->outlines.count(*gid)) return false;
            setFontData(detail::removeOutlineGlyph(state->data, *gid));
            return true;
        }
        auto svg = state->svg;
        svg.glyphs.erase(*gid);
        if (state->outlines.count(*gid)) {
            auto bytes = detail::removeOutlineGlyph(state->data, *gid);
            setFontData(utils::rebuildSfnt(bytes, {{"SVG ", svg.build()}}));
        } else apply(std::move(svg));
        return true;
    }
    bool removeGlyph(uint32_t unicode) override {
        const auto name = findGlyphName(unicode);
        return !name.empty() && removeGlyph(name);
    }
    bool replaceGlyphImage(const std::string& name, const std::vector<uint8_t>& image) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid) return false;
        if (!state->svg.glyphs.count(*gid) && !state->outlines.count(*gid)) return false;
        auto svg = state->svg;
        if (!svg.replace(*gid, image, state->metadata.upem)) return false;
        apply(std::move(svg));
        return true;
    }
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        auto ids = state->outlines;
        for (const auto& entry : state->svg.glyphs) ids.insert(entry.first);
        result.reserve(ids.size());
        for (const auto gid : ids) result.push_back(info(gid));
        return result;
    }
    GlyphInfo getGlyphInfo(const std::string& name) const override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || (!state->svg.glyphs.count(*gid) && !state->outlines.count(*gid))) throw GlyphNotFoundException(name);
        return info(*gid);
    }
    std::string findGlyphName(uint32_t unicode) const override {
        const auto found = state->metadata.chars.find(unicode);
        return found != state->metadata.chars.end() && (state->svg.glyphs.count(found->second) || state->outlines.count(found->second))
            ? state->metadata.name(found->second) : "";
    }
    bool save(const std::string& path) override {
        if (!detail::writeFontFile(path, state->data)) throw FontSaveException(path, "Cannot write font data");
        return true;
    }
};

class SVGHandler final : public FontFormatHandler {
public:
    bool canHandle(const std::string& path) override {
        try { return utils::hasTable(utils::parseTTFTables(utils::readFontFile(path)), "SVG "); }
        catch (const std::exception&) { return false; }
    }
    std::unique_ptr<Font> loadFont(const std::string& path) override { return std::make_unique<SVGFont>(path); }
    FontFormat getFormat() const override { return FontFormat::SVG; }
};
} // namespace
void registerSVGHandler() { registerHandler(std::make_unique<SVGHandler>()); }
} // namespace fontmaster
