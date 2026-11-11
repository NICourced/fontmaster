#include "StandardFont.h"
#include "SVGTable.h"
#include "fontmaster/TTFUtils.h"
#include "utils/GlyphMetadata.h"
#include <algorithm>
#include <hb-ot.h>
#include <hb-subset.h>
#include <limits>
#include <set>

namespace fontmaster::detail {
namespace {
using Bytes = std::vector<uint8_t>;
template <class T, void (*Destroy)(T *)> using Owner = std::unique_ptr<T, decltype(Destroy)>;
[[noreturn]] void invalid(const std::string &reason) { throw FontFormatException("outline", reason); }
Owner<hb_face_t, hb_face_destroy> openFace(const Bytes &data) {
    if (data.size() > std::numeric_limits<unsigned>::max())
        invalid("Font exceeds HarfBuzz input size limit");
    Owner<hb_blob_t, hb_blob_destroy> blob(hb_blob_create(reinterpret_cast<const char *>(data.data()),
                                                          static_cast<unsigned>(data.size()),
                                                          HB_MEMORY_MODE_READONLY, nullptr, nullptr),
                                           hb_blob_destroy);
    Owner<hb_face_t, hb_face_destroy> face(hb_face_create(blob.get(), 0), hb_face_destroy);
    if (!face || !hb_face_get_glyph_count(face.get()))
        invalid("Cannot read outline font");
    return face;
}
Owner<hb_font_t, hb_font_destroy> openFont(hb_face_t *face) {
    Owner<hb_font_t, hb_font_destroy> font(hb_font_create(face), hb_font_destroy);
    hb_ot_font_set_funcs(font.get());
    const auto upem = hb_face_get_upem(face);
    hb_font_set_scale(font.get(), upem, upem);
    return font;
}
uint16_t read16(const Bytes &data, size_t p) {
    if (p > data.size() || data.size() - p < 2)
        invalid("Truncated outline table");
    return (uint16_t(data[p]) << 8) | data[p + 1];
}
uint32_t read32(const Bytes &data, size_t p) {
    if (p > data.size() || data.size() - p < 4)
        invalid("Truncated outline table");
    return (uint32_t(data[p]) << 24) | (uint32_t(data[p + 1]) << 16) | (uint32_t(data[p + 2]) << 8) |
           data[p + 3];
}
void validateOutlines(const Bytes &data, uint16_t count) {
    const auto tables = utils::parseTTFTables(data);
    if (!count)
        invalid("Font has no glyphs");
    const auto head = tableBytes(data, "head");
    if (head.size() < 54 || !read16(head, 18))
        invalid("Missing or invalid head table");
    if (utils::hasTable(tables, "glyf")) {
        const auto glyf = tableBytes(data, "glyf");
        const auto loca = tableBytes(data, "loca");
        const auto format = read16(head, 50);
        if (format > 1)
            invalid("Unsupported loca offset format");
        const size_t entrySize = format ? 4 : 2;
        if (loca.size() < (size_t(count) + 1) * entrySize)
            invalid("Truncated loca table");
        size_t previous = 0;
        for (size_t gid = 0; gid <= count; ++gid) {
            const size_t next = format ? read32(loca, gid * 4) : size_t(read16(loca, gid * 2)) * 2;
            if (next < previous || next > glyf.size())
                invalid("Invalid glyf offset");
            if (gid && next != previous && next - previous < 10)
                invalid("Truncated glyf record");
            previous = next;
        }
    } else {
        const auto cff = tableBytes(data, "CFF ");
        const auto cff2 = tableBytes(data, "CFF2");
        if (cff.empty() && cff2.empty())
            invalid("No TrueType, CFF or CFF2 outlines");
        if ((!cff.empty() && (cff.size() < 4 || cff[0] != 1)) ||
            (!cff2.empty() && (cff2.size() < 5 || cff2[0] != 2)))
            invalid("Invalid CFF header");
    }
}
bool isWhitespace(uint32_t code) {
    return (code >= 9 && code <= 13) || code == 32 || code == 0x85 || code == 0xA0 || code == 0x1680 ||
           (code >= 0x2000 && code <= 0x200A) || code == 0x2028 || code == 0x2029 || code == 0x202F ||
           code == 0x205F || code == 0x3000;
}
std::set<uint16_t> collectOutlines(const GlyphMetadata &metadata, hb_font_t *font) {
    std::set<uint16_t> result;
    for (size_t gid = 0; gid < metadata.count; ++gid) {
        hb_glyph_extents_t extents{};
        if (hb_font_get_glyph_extents(font, gid, &extents) && (extents.width || extents.height))
            result.insert(static_cast<uint16_t>(gid));
    }
    for (const auto &entry : metadata.chars)
        if (isWhitespace(entry.first))
            result.insert(entry.second);
    return result;
}
struct State {
    Bytes data;
    GlyphMetadata metadata;
    SVGTable svg;
    std::set<uint16_t> active;
    explicit State(const Bytes &bytes) : data(bytes), metadata(bytes) {
        validateOutlines(data, metadata.count);
        auto face = openFace(data);
        if (hb_face_get_glyph_count(face.get()) != metadata.count)
            invalid("Inconsistent outline glyph count");
        auto font = openFont(face.get());
        active = collectOutlines(metadata, font.get());
        const auto svgBytes = tableBytes(data, "SVG ");
        if (!svgBytes.empty())
            svg = SVGTable(svgBytes, metadata.count);
        for (const auto &entry : svg.glyphs)
            active.insert(entry.first);
    }
};
class StandardFont final : public Font {
    FontFormat format;
    std::unique_ptr<State> state;
    GlyphInfo info(uint16_t gid, bool includeImageData = true) const {
        GlyphInfo result{};
        result.name = state->metadata.name(gid);
        result.unicode = state->metadata.unicode(gid);
        const auto found = state->svg.glyphs.find(gid);
        if (found != state->svg.glyphs.end()) {
            result.format = "svg";
            if (includeImageData) result.image_data = *found->second;
            result.data_size = found->second->size();
        } else
            result.format = "outline";
        return result;
    }

  public:
    StandardFont(const std::string &path, FontFormat format) : format(format) {
        setFontData(utils::readFontFile(path));
    }
    bool load() override { return true; }
    FontFormat getFormat() const override { return format; }
    const Bytes &getFontData() const override { return state->data; }
    void setFontData(const Bytes &data) override {
        try {
            auto candidate = std::make_unique<State>(data);
            state = std::move(candidate);
        } catch (const FontException &) {
            throw;
        } catch (const std::exception &error) {
            invalid(error.what());
        }
    }
    bool save(const std::string &path) override {
        if (!writeFontFile(path, state->data))
            throw FontSaveException(path, "Cannot write font data");
        return true;
    }
  private:
    std::vector<GlyphInfo> glyphList(bool includeImageData) const {
        std::vector<GlyphInfo> result;
        result.reserve(state->active.size());
        for (const auto gid : state->active)
            result.push_back(info(gid, includeImageData));
        return result;
    }
  public:
    std::vector<GlyphInfo> listGlyphs() const override { return glyphList(true); }
    std::vector<GlyphInfo> listGlyphMetadata() const override { return glyphList(false); }
    GlyphInfo getGlyphInfo(const std::string &name) const override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->active.count(*gid))
            throw GlyphNotFoundException(name);
        return info(*gid);
    }
    std::string findGlyphName(uint32_t unicode) const override {
        const auto it = state->metadata.chars.find(unicode);
        return it != state->metadata.chars.end() && state->active.count(it->second)
                   ? state->metadata.name(it->second)
                   : "";
    }
    bool removeGlyph(uint32_t unicode) override {
        const auto name = findGlyphName(unicode);
        return !name.empty() && removeGlyph(name);
    }
    bool removeGlyph(const std::string &name) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->active.count(*gid))
            return false;
        auto bytes = removeOutlineGlyph(state->data, *gid);
        auto svg = state->svg;
        if (svg.glyphs.erase(*gid))
            bytes = utils::rebuildSfnt(bytes, {{"SVG ", svg.build()}});
        setFontData(bytes);
        return true;
    }
    bool replaceGlyphImage(const std::string &name, const Bytes &image) override {
        const auto gid = state->metadata.resolve(name);
        if (!gid || !state->active.count(*gid))
            return false;
        auto svg = state->svg;
        if (!svg.replace(*gid, image, state->metadata.upem))
            return false;
        auto bytes = utils::rebuildSfnt(state->data, {{"SVG ", svg.build()}});
        setFontData(bytes);
        return true;
    }
};
} // namespace
std::set<uint16_t> outlineGlyphs(const Bytes &data) {
    const GlyphMetadata metadata(data);
    validateOutlines(data, metadata.count);
    auto face = openFace(data);
    if (hb_face_get_glyph_count(face.get()) != metadata.count)
        invalid("Inconsistent outline glyph count");
    auto font = openFont(face.get());
    return collectOutlines(metadata, font.get());
}
Bytes removeOutlineGlyph(const Bytes &data, uint16_t gid) {
    const GlyphMetadata metadata(data);
    validateOutlines(data, metadata.count);
    if (gid >= metadata.count)
        invalid("Glyph ID outside font");
    if (!gid)
        invalid("The .notdef glyph cannot be removed");
    auto face = openFace(data);
    Owner<hb_subset_input_t, hb_subset_input_destroy> input(hb_subset_input_create_or_fail(),
                                                            hb_subset_input_destroy);
    if (!input)
        invalid("Cannot allocate subset input");
    hb_subset_input_keep_everything(input.get());
    auto *glyphs = hb_subset_input_glyph_set(input.get());
    hb_set_clear(glyphs);
    hb_set_add_range(glyphs, 0, metadata.count - 1);
    hb_set_del(glyphs, gid);
    auto *unicodes = hb_subset_input_unicode_set(input.get());
    for (const auto &entry : metadata.chars)
        if (entry.second == gid)
            hb_set_del(unicodes, entry.first);
    hb_subset_input_set_flags(input.get(), HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_GLYPH_NAMES |
                                               HB_SUBSET_FLAGS_NOTDEF_OUTLINE | HB_SUBSET_FLAGS_NAME_LEGACY |
                                               HB_SUBSET_FLAGS_NO_LAYOUT_CLOSURE |
                                               HB_SUBSET_FLAGS_NO_BIDI_CLOSURE |
                                               HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
    // SVG document IDs remain valid because all other glyph IDs are retained.
    // Callers update presentation coverage independently of outline removal.
    hb_set_add(hb_subset_input_set(input.get(), HB_SUBSET_SETS_NO_SUBSET_TABLE_TAG),
               HB_TAG('S', 'V', 'G', ' '));
    Owner<hb_subset_plan_t, hb_subset_plan_destroy> plan(
        hb_subset_plan_create_or_fail(face.get(), input.get()), hb_subset_plan_destroy);
    if (!plan)
        invalid("Cannot plan safe glyph removal");
    auto *mapping = hb_subset_plan_old_to_new_glyph_mapping(plan.get());
    if (hb_map_has(mapping, gid))
        invalid("Glyph is required by another glyph; removal would break its outline");
    for (size_t other = 0; other < metadata.count; ++other)
        if (other != gid && (!hb_map_has(mapping, other) || hb_map_get(mapping, other) != other))
            invalid("Glyph removal would change unrelated glyph IDs");
    Owner<hb_face_t, hb_face_destroy> subset(hb_subset_plan_execute_or_fail(plan.get()), hb_face_destroy);
    if (!subset)
        invalid("Cannot subset font safely");
    Owner<hb_blob_t, hb_blob_destroy> blob(hb_face_reference_blob(subset.get()), hb_blob_destroy);
    unsigned length = 0;
    const auto *bytes = reinterpret_cast<const uint8_t *>(hb_blob_get_data(blob.get(), &length));
    if (!bytes || !length)
        invalid("Subset produced no font data");
    Bytes result(bytes, bytes + length);
    const GlyphMetadata updated(result);
    // RETAIN_GIDS preserves every surviving ID. HarfBuzz may omit the removed
    // final slot; it must never truncate or renumber any unrelated glyph.
    if (updated.count != metadata.count &&
        !(size_t(gid) + 1 == metadata.count && size_t(updated.count) + 1 == metadata.count))
        invalid("Glyph removal changed unrelated glyph IDs");
    for (const auto &entry : updated.chars)
        if (entry.second == gid)
            invalid("Subset retained the removed Unicode mapping");
    return result;
}
std::unique_ptr<Font> createStandardFont(const std::string &path, FontFormat format) {
    return std::make_unique<StandardFont>(path, format);
}
} // namespace fontmaster::detail
