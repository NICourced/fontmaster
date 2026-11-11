#include "../utils/GlyphMetadata.h"
#include "../utils/ImageCodec.h"
#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace fontmaster {
namespace {
using Bytes = std::vector<uint8_t>;
[[noreturn]] void invalid(const std::string &reason) { throw FontFormatException("SBIX", reason); }
uint16_t read16(const Bytes &bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2)
        invalid("Truncated 16-bit field");
    return (uint16_t(bytes[offset]) << 8) | bytes[offset + 1];
}
uint32_t read32(const Bytes &bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        invalid("Truncated 32-bit field");
    return (uint32_t(bytes[offset]) << 24) | (uint32_t(bytes[offset + 1]) << 16) |
           (uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
}
uint32_t offset32(size_t offset) {
    if (offset > std::numeric_limits<uint32_t>::max())
        invalid("Table exceeds 32-bit size limit");
    return static_cast<uint32_t>(offset);
}
void put16(Bytes &bytes, size_t offset, uint16_t value) {
    bytes.at(offset) = value >> 8;
    bytes.at(offset + 1) = value;
}
void put32(Bytes &bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.at(offset + i) = value >> (24 - 8 * i);
}
struct Record {
    int16_t x = 0, y = 0;
    std::string type;
    Bytes payload;
    bool empty() const { return type.empty(); }
};
struct Strike {
    uint16_t ppem = 0, resolution = 0;
    std::vector<Record> records;
    // Each populated entry points to the terminal non-duplicate image.
    std::vector<size_t> resolved;
};
void resolveDuplicates(Strike &strike) {
    const auto count = strike.records.size();
    strike.resolved.assign(count, count);
    std::vector<uint8_t> state(count, 0);
    for (size_t gid = 0; gid < count; ++gid) {
        if (state[gid] == 2 || strike.records[gid].empty())
            continue;
        std::vector<size_t> path;
        size_t current = gid;
        while (true) {
            if (current >= count || strike.records[current].empty())
                invalid("Duplicate references a missing glyph");
            if (state[current] == 1)
                invalid("Cyclic duplicate glyph reference");
            if (state[current] == 2)
                break;
            state[current] = 1;
            path.push_back(current);
            const auto &record = strike.records[current];
            if (record.type != "dupe") {
                strike.resolved[current] = current;
                break;
            }
            if (record.payload.size() != 2)
                invalid("Duplicate record must contain one glyph ID");
            current = read16(record.payload, 0);
        }
        const auto terminal = strike.resolved[current];
        for (auto node : path) {
            strike.resolved[node] = terminal;
            state[node] = 2;
        }
    }
}
struct State {
    Bytes font;
    detail::GlyphMetadata metadata;
    uint16_t flags = 0;
    std::vector<Strike> strikes;
    explicit State(const Bytes &data) : font(data), metadata(data) {
        const auto table = detail::tableBytes(data, "sbix");
        if (table.size() < 8)
            invalid("Missing or truncated sbix table");
        if (read16(table, 0) != 1)
            invalid("Only sbix version 1 is supported");
        flags = read16(table, 2);
        const auto count = read32(table, 4);
        if (count > (table.size() - 8) / 4)
            invalid("Truncated strike offset array");
        const size_t directoryEnd = 8 + size_t(count) * 4;
        const size_t strikeHeaderSize = 4 + (size_t(metadata.count) + 1) * 4;
        for (size_t index = 0; index < count; ++index) {
            const size_t start = read32(table, 8 + 4 * index);
            const size_t end = index + 1 < count ? read32(table, 12 + 4 * index) : table.size();
            if (start < directoryEnd || end < start || end > table.size() || end - start < strikeHeaderSize)
                invalid("Invalid strike bounds");
            Strike strike;
            strike.ppem = read16(table, start);
            strike.resolution = read16(table, start + 2);
            strike.records.resize(metadata.count);
            size_t previous = read32(table, start + 4);
            if (previous < strikeHeaderSize || previous > end - start)
                invalid("Invalid first glyph offset");
            for (size_t gid = 0; gid < metadata.count; ++gid) {
                const size_t next = read32(table, start + 8 + gid * 4);
                if (next < previous || next > end - start)
                    invalid("Invalid glyph offset or sentinel");
                if (next != previous) {
                    if (next - previous < 8)
                        invalid("Truncated glyph record");
                    const size_t p = start + previous;
                    auto &record = strike.records[gid];
                    record.x = static_cast<int16_t>(read16(table, p));
                    record.y = static_cast<int16_t>(read16(table, p + 2));
                    record.type.assign(table.begin() + p + 4, table.begin() + p + 8);
                    record.payload.assign(table.begin() + p + 8, table.begin() + start + next);
                    if (record.type != "png " && record.type != "jpg " && record.type != "tiff" &&
                        record.type != "dupe")
                        invalid("Unsupported graphic type: " + record.type);
                    if (record.payload.empty())
                        invalid("Empty image payload");
                }
                previous = next;
            }
            resolveDuplicates(strike);
            strikes.push_back(std::move(strike));
        }
    }
};
Bytes buildTable(const State &state, const std::vector<Strike> &strikes) {
    Bytes table(8 + strikes.size() * 4);
    put16(table, 0, 1);
    put16(table, 2, state.flags);
    put32(table, 4, offset32(strikes.size()));
    for (size_t i = 0; i < strikes.size(); ++i) {
        const auto &strike = strikes[i];
        const size_t start = table.size();
        put32(table, 8 + i * 4, offset32(start));
        table.resize(start + 4 + (strike.records.size() + 1) * 4);
        put16(table, start, strike.ppem);
        put16(table, start + 2, strike.resolution);
        for (size_t gid = 0; gid < strike.records.size(); ++gid) {
            put32(table, start + 4 + gid * 4, offset32(table.size() - start));
            const auto &record = strike.records[gid];
            if (record.empty())
                continue;
            const size_t p = table.size();
            table.resize(p + 8);
            put16(table, p, static_cast<uint16_t>(record.x));
            put16(table, p + 2, static_cast<uint16_t>(record.y));
            std::copy(record.type.begin(), record.type.end(), table.begin() + p + 4);
            table.insert(table.end(), record.payload.begin(), record.payload.end());
        }
        put32(table, start + 4 + strike.records.size() * 4, offset32(table.size() - start));
    }
    return table;
}
class SBIX_Font final : public Font {
    State state;
    bool contains(size_t gid) const {
        return std::any_of(state.strikes.begin(), state.strikes.end(),
                           [gid](const Strike &strike) { return !strike.records[gid].empty(); });
    }
    void commit(const std::vector<Strike> &strikes) {
        auto bytes = utils::rebuildSfnt(state.font, {{"sbix", buildTable(state, strikes)}});
        State candidate(bytes);
        state = std::move(candidate);
    }
    GlyphInfo info(uint16_t gid, bool includeImageData = true) const {
        for (const auto &strike : state.strikes) {
            if (strike.records[gid].empty())
                continue;
            const auto &record = strike.records[strike.resolved[gid]];
            GlyphInfo result{};
            result.name = state.metadata.name(gid);
            result.unicode = state.metadata.unicode(gid);
            if (includeImageData) result.image_data = record.payload;
            result.data_size = record.payload.size();
            result.format = record.type == "png " ? "png" : record.type == "jpg " ? "jpg" : "tiff";
            return result;
        }
        throw GlyphNotFoundException(state.metadata.name(gid));
    }

  public:
    explicit SBIX_Font(const std::string &path) : state(utils::readFontFile(path)) {}
    bool load() override { return true; }
    FontFormat getFormat() const override { return FontFormat::SBIX; }
    const Bytes &getFontData() const override { return state.font; }
    void setFontData(const Bytes &data) override {
        State candidate(data);
        state = std::move(candidate);
    }
    bool save(const std::string &path) override {
        if (!detail::writeFontFile(path, state.font))
            throw FontSaveException(path, "Cannot write font data");
        return true;
    }
  private:
    std::vector<GlyphInfo> glyphList(bool includeImageData) const {
        std::vector<GlyphInfo> result;
        for (size_t gid = 0; gid < state.metadata.count; ++gid)
            if (contains(gid))
                result.push_back(info(static_cast<uint16_t>(gid), includeImageData));
        return result;
    }
  public:
    std::vector<GlyphInfo> listGlyphs() const override { return glyphList(true); }
    std::vector<GlyphInfo> listGlyphMetadata() const override { return glyphList(false); }
    GlyphInfo getGlyphInfo(const std::string &name) const override {
        const auto gid = state.metadata.resolve(name);
        if (!gid)
            throw GlyphNotFoundException(name);
        return info(*gid);
    }
    std::string findGlyphName(uint32_t unicode) const override {
        const auto it = state.metadata.chars.find(unicode);
        return it != state.metadata.chars.end() && contains(it->second) ? state.metadata.name(it->second)
                                                                        : "";
    }
    bool removeGlyph(uint32_t unicode) override {
        const auto name = findGlyphName(unicode);
        return !name.empty() && removeGlyph(name);
    }
    bool removeGlyph(const std::string &name) override {
        const auto gid = state.metadata.resolve(name);
        if (!gid || !contains(*gid))
            return false;
        auto strikes = state.strikes;
        for (auto &strike : strikes) {
            if (strike.records[*gid].empty())
                continue;
            // Materialize every live reference that would otherwise become dangling,
            // retaining each duplicate's own origin and every unrelated raw record.
            std::vector<std::vector<size_t>> references(strike.records.size());
            for (size_t i = 0; i < strike.records.size(); ++i)
                if (strike.records[i].type == "dupe")
                    references[read16(strike.records[i].payload, 0)].push_back(i);
            std::vector<size_t> affected{*gid};
            for (size_t i = 0; i < affected.size(); ++i)
                affected.insert(affected.end(), references[affected[i]].begin(),
                                references[affected[i]].end());
            for (size_t i = 1; i < affected.size(); ++i) {
                auto &record = strike.records[affected[i]];
                const auto &source = strike.records[strike.resolved[affected[i]]];
                record.type = source.type;
                record.payload = source.payload;
            }
            strike.records[*gid] = Record{};
        }
        commit(strikes);
        return true;
    }
    bool replaceGlyphImage(const std::string &name, const Bytes &image) override {
        const auto gid = state.metadata.resolve(name);
        if (!gid || !contains(*gid))
            return false;
        try {
            detail::decodePNG(image);
        } catch (const std::exception &) {
            return false;
        }
        auto strikes = state.strikes;
        for (auto &strike : strikes)
            if (!strike.records[*gid].empty()) {
                strike.records[*gid].type = "png ";
                strike.records[*gid].payload = image;
            }
        commit(strikes);
        return true;
    }
};
class SBIX_Handler final : public FontFormatHandler {
  public:
    bool canHandle(const std::string &path) override {
        try {
            return utils::hasTable(utils::parseTTFTables(utils::readFontFile(path)), "sbix");
        } catch (const std::exception &) {
            return false;
        }
    }
    std::unique_ptr<Font> loadFont(const std::string &path) override {
        return std::make_unique<SBIX_Font>(path);
    }
    FontFormat getFormat() const override { return FontFormat::SBIX; }
};
} // namespace
void registerSBIXHandler() { registerHandler(std::make_unique<SBIX_Handler>()); }
} // namespace fontmaster
