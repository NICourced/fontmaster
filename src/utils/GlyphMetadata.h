#pragma once
#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/CMAPParser.h"
#include <optional>
#include <fstream>
#include <hb.h>
#include <hb-ot.h>

namespace fontmaster::detail {
inline std::vector<uint8_t> tableBytes(const std::vector<uint8_t>& data, const std::string& tag) {
    const auto tables = utils::parseTTFTables(data);
    const auto* table = utils::findTable(tables, tag);
    return table ? std::vector<uint8_t>(data.begin() + table->offset, data.begin() + table->offset + table->length)
                 : std::vector<uint8_t>{};
}
struct GlyphMetadata {
    uint16_t count = 0;
    uint16_t upem = 1000;
    std::map<uint16_t, std::string> names;
    std::map<uint32_t, uint16_t> chars;
    std::map<uint16_t, uint32_t> codes;
    explicit GlyphMetadata(const std::vector<uint8_t>& data) {
        const auto maxp = tableBytes(data, "maxp");
        if (maxp.size() < 6) throw FontFormatException("sfnt", "Missing or truncated maxp table");
        count = (uint16_t(maxp[4]) << 8) | maxp[5];
        const auto head = tableBytes(data, "head");
        if (head.size() >= 20) upem = (uint16_t(head[18]) << 8) | head[19];
        const auto post = tableBytes(data, "post");
        if (!post.empty()) {
            utils::POSTParser parser(post, 0, count);
            if (parser.parse()) names = parser.getGlyphNames();
        }
        // CFF stores real glyph names in its charset even when POST is v3.
        // Keep generated aliases only when the font does not supply a name.
        if (data.size() > UINT32_MAX) throw FontFormatException("sfnt", "Font data exceeds 32-bit size");
        const auto destroyBlob = [](hb_blob_t* blob) { hb_blob_destroy(blob); };
        const auto destroyFace = [](hb_face_t* face) { hb_face_destroy(face); };
        const auto destroyFont = [](hb_font_t* font) { hb_font_destroy(font); };
        std::unique_ptr<hb_blob_t, decltype(destroyBlob)> blob(hb_blob_create(
            reinterpret_cast<const char*>(data.data()), data.size(), HB_MEMORY_MODE_READONLY, nullptr, nullptr), destroyBlob);
        std::unique_ptr<hb_face_t, decltype(destroyFace)> face(hb_face_create(blob.get(), 0), destroyFace);
        std::unique_ptr<hb_font_t, decltype(destroyFont)> font(hb_font_create(face.get()), destroyFont);
        hb_ot_font_set_funcs(font.get());
        for (uint32_t gid = 0; gid < count; ++gid) {
            char glyphName[256];
            if (hb_font_get_glyph_name(font.get(), gid, glyphName, sizeof(glyphName))) names[gid] = glyphName;
        }
        const auto cmap = tableBytes(data, "cmap");
        if (!cmap.empty()) {
            utils::CMAPParser parser(cmap);
            if (!parser.parse()) throw FontFormatException("sfnt", "Invalid cmap table");
            chars = parser.getCharToGlyphMap();
            for (const auto& entry : chars) {
                if (entry.second >= count) throw FontFormatException("sfnt", "cmap references an invalid glyph");
                codes.emplace(entry.second, entry.first);
            }
        }
    }
    std::string name(uint16_t gid) const {
        const auto it = names.find(gid);
        return it == names.end() ? "glyph_" + std::to_string(gid) : it->second;
    }
    uint32_t unicode(uint16_t gid) const {
        const auto it = codes.find(gid);
        return it == codes.end() ? 0 : it->second;
    }
    std::optional<uint16_t> resolve(const std::string& name) const {
        for (const auto& entry : names) if (entry.second == name) return entry.first;
        const size_t prefix = name.rfind("glyph_", 0) == 0 ? 6 : name.rfind("glyph", 0) == 0 ? 5 : 0;
        if (prefix) {
            const auto number = name.substr(prefix);
            if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos) return {};
            try {
                const auto value = std::stoul(number);
                if (value < count) return static_cast<uint16_t>(value);
            } catch (const std::exception&) {}
        }
        return {};
    }
};
inline bool writeFontFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    return file && file.write(reinterpret_cast<const char*>(data.data()), data.size()).good();
}
} // namespace fontmaster::detail
