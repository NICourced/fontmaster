#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/TTFUtils.h"
#include "utils/ImageCodec.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace fontmaster {
namespace {
// Table-local bounds prevent an invalid offset from reading another table.
struct View {
    const uint8_t* data;
    size_t size;
    void check(size_t offset, size_t length) const {
        if (offset > size || length > size - offset)
            throw std::runtime_error("CBDT/CBLC record outside table bounds");
    }
    uint16_t u16(size_t offset) const {
        check(offset, 2);
        return (uint16_t(data[offset]) << 8) | data[offset + 1];
    }
    uint32_t u32(size_t offset) const {
        check(offset, 4);
        return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) |
               (uint32_t(data[offset + 2]) << 8) | data[offset + 3];
    }
};
void append32(std::vector<uint8_t>& data, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) data.push_back(value >> shift);
}
bool validPNG(const std::vector<uint8_t>& png) {
    static constexpr std::array<uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (png.size() < 33 || !std::equal(signature.begin(), signature.end(), png.begin())) return false;
    View view{png.data(), png.size()};
    if (view.u32(8) != 13 || view.u32(12) != 0x49484452 ||
        view.u32(16) == 0 || view.u32(20) == 0) return false;
    bool hasData = false;
    size_t offset = 8;
    while (offset <= png.size() - 12) {
        const size_t length = view.u32(offset);
        if (length > png.size() - offset - 12) return false;
        const uint32_t tag = view.u32(offset + 4);
        if (tag == 0x49444154) hasData = true;
        offset += length + 12;
        if (tag == 0x49454E44) return length == 0 && hasData && offset == png.size();
    }
    return false;
}
void extractImage(const View& cbdt, uint32_t offset, uint32_t length,
                  uint16_t format, const std::vector<uint8_t>& sharedMetrics,
                  uint16_t gid, StrikeRecord& strike) {
    if (length == 0) return;
    cbdt.check(offset, length);
    GlyphImage image;
    image.glyphID = gid;
    image.imageFormat = format;
    image.offset = offset;
    image.length = length;
    image.recordData.assign(cbdt.data + offset, cbdt.data + offset + length);
    size_t metricsSize = 0;
    switch (format) {
        case 1: case 2: case 8: case 17: metricsSize = 5; break;
        case 6: case 7: case 9: case 18: metricsSize = 8; break;
        case 5: case 19: break;
        default: throw std::runtime_error("Unsupported CBDT image format " + std::to_string(format));
    }
    if (length < metricsSize) throw std::runtime_error("Truncated bitmap metrics");
    const uint8_t* metrics = metricsSize ? image.recordData.data() : sharedMetrics.data();
    if (metricsSize || sharedMetrics.size() == 8) {
        image.height = metrics[0];
        image.width = metrics[1];
        image.bearingX = static_cast<int8_t>(metrics[2]);
        image.bearingY = static_cast<int8_t>(metrics[3]);
        image.advance = metrics[4];
    }
    size_t payload = metricsSize;
    size_t payloadLength = length - payload;
    if (format >= 17) {
        View record{image.recordData.data(), image.recordData.size()};
        payloadLength = record.u32(payload);
        payload += 4;
        record.check(payload, payloadLength);
    }
    image.data.assign(image.recordData.begin() + payload,
                      image.recordData.begin() + payload + payloadLength);
    if (format == 8 || format == 9) {
        const View record{image.recordData.data(), image.recordData.size()};
        const size_t countOffset = format == 8 ? 6 : 8;
        const size_t count = record.u16(countOffset);
        record.check(countOffset + 2, count * 4);
    }
    if (format >= 17 && !validPNG(image.data)) throw std::runtime_error("Invalid CBDT PNG payload");
    // Embed shared metrics to allow variable-length index subtables on save.
    if (format == 5 || format == 19) {
        if (sharedMetrics.size() != 8) throw std::runtime_error("Missing shared bitmap metrics");
        image.imageFormat = format == 5 ? 7 : 18;
        image.recordData = sharedMetrics;
        if (format == 19) append32(image.recordData, image.data.size());
        image.recordData.insert(image.recordData.end(), image.data.begin(), image.data.end());
    }
    strike.glyphIDs.push_back(gid);
    if (!strike.glyphImages.emplace(gid, std::move(image)).second)
        throw std::runtime_error("Overlapping CBLC glyph ranges");
}
} // namespace

bool CBDT_CBLC_Parser::parse() {
    strikes.clear();
    removedGlyphs.clear();
    try {
        const auto tables = utils::parseTTFTables(fontData);
        const auto* location = utils::findTable(tables, "CBLC");
        const auto* bitmap = utils::findTable(tables, "CBDT");
        if (!location || !bitmap) throw std::runtime_error("Missing CBDT or CBLC table");
        View file{fontData.data(), fontData.size()};
        file.check(location->offset, location->length);
        file.check(bitmap->offset, bitmap->length);
        View cblc{fontData.data() + location->offset, location->length};
        View cbdt{fontData.data() + bitmap->offset, bitmap->length};
        // Android EmojiCompat also ships the legacy 2.0 version marker.
        if ((cblc.u32(0) != 0x00030000 && cblc.u32(0) != 0x00020000) ||
            (cbdt.u32(0) != 0x00030000 && cbdt.u32(0) != 0x00020000))
            throw std::runtime_error("Unsupported CBDT/CBLC version");
        const size_t count = cblc.u32(4);
        if (count > 65535) throw std::runtime_error("Too many bitmap strikes");
        cblc.check(8, count * 48);
        for (size_t index = 0; index < count; ++index) {
            const size_t offset = 8 + index * 48;
            StrikeRecord strike;
            strike.bitmapSizeTable.assign(cblc.data + offset, cblc.data + offset + 48);
            strike.ppem = cblc.data[offset + 45];
            const size_t arrayOffset = cblc.u32(offset);
            const size_t arraySize = cblc.u32(offset + 4);
            const size_t subtableCount = cblc.u32(offset + 8);
            cblc.check(arrayOffset, arraySize);
            View array{cblc.data + arrayOffset, arraySize};
            array.check(0, subtableCount * 8);
            for (size_t i = 0; i < subtableCount; ++i) {
                const uint16_t first = array.u16(i * 8), last = array.u16(i * 8 + 2);
                if (first > last) throw std::runtime_error("Invalid CBLC glyph range");
                const size_t sub = array.u32(i * 8 + 4);
                const uint16_t indexFormat = array.u16(sub), imageFormat = array.u16(sub + 2);
                const uint32_t imageOffset = array.u32(sub + 4);
                auto emit = [&](uint16_t gid, uint32_t begin, uint32_t end,
                                const std::vector<uint8_t>& metrics = {}) {
                    if (end < begin || imageOffset > cbdt.size || end > cbdt.size - imageOffset)
                        throw std::runtime_error("Invalid CBDT image offsets");
                    extractImage(cbdt, imageOffset + begin, end - begin, imageFormat, metrics, gid, strike);
                };
                if (indexFormat == 1 || indexFormat == 3) {
                    const size_t width = indexFormat == 1 ? 4 : 2;
                    array.check(sub + 8, (size_t(last) - first + 2) * width);
                    auto read = [&](size_t n) { return width == 4 ? array.u32(sub + 8 + n * width) : array.u16(sub + 8 + n * width); };
                    for (uint32_t gid = first; gid <= last; ++gid) emit(gid, read(gid - first), read(gid - first + 1));
                } else if (indexFormat == 2 || indexFormat == 5) {
                    const size_t imageSize = array.u32(sub + 8);
                    array.check(sub + 12, 8);
                    std::vector<uint8_t> metrics(array.data + sub + 12, array.data + sub + 20);
                    const size_t n = indexFormat == 2 ? size_t(last) - first + 1 : array.u32(sub + 20);
                    if (n > 65536 || (imageSize && n > (cbdt.size - std::min<size_t>(imageOffset, cbdt.size)) / imageSize))
                        throw std::runtime_error("Invalid fixed-size CBLC images");
                    if (indexFormat == 5) array.check(sub + 24, n * 2);
                    for (size_t j = 0; j < n; ++j) {
                        const uint16_t gid = indexFormat == 2 ? first + j : array.u16(sub + 24 + j * 2);
                        if (gid < first || gid > last) throw std::runtime_error("Sparse glyph outside CBLC range");
                        emit(gid, j * imageSize, (j + 1) * imageSize, metrics);
                    }
                } else if (indexFormat == 4) {
                    const size_t n = array.u32(sub + 8);
                    if (n > 65536) throw std::runtime_error("Too many sparse glyphs");
                    array.check(sub + 12, (n + 1) * 4);
                    for (size_t j = 0; j < n; ++j) {
                        const uint16_t gid = array.u16(sub + 12 + j * 4);
                        if (gid < first || gid > last) throw std::runtime_error("Sparse glyph outside CBLC range");
                        emit(gid, array.u16(sub + 14 + j * 4), array.u16(sub + 18 + j * 4));
                    }
                } else {
                    throw std::runtime_error("Unsupported CBLC index format " + std::to_string(indexFormat));
                }
            }
            strikes.emplace(index, std::move(strike));
        }
        return true;
    } catch (const std::exception& e) {
        strikes.clear();
        std::cerr << "CBDT/CBLC: " << e.what() << '\n';
        return false;
    }
}

bool CBDT_CBLC_Parser::removeGlyph(uint16_t glyphID) {
    // Composite bitmap records can reference other bitmaps in the same strike.
    // Check all strikes before mutating any of them.
    for (const auto& entry : strikes) {
        for (const auto& item : entry.second.glyphImages) {
            const auto& image = item.second;
            if (item.first == glyphID || (image.imageFormat != 8 && image.imageFormat != 9)) continue;
            const View record{image.recordData.data(), image.recordData.size()};
            const size_t countOffset = image.imageFormat == 8 ? 6 : 8;
            const size_t count = record.u16(countOffset);
            for (size_t i = 0; i < count; ++i)
                if (record.u16(countOffset + 2 + i * 4) == glyphID) return false;
        }
    }
    bool removed = false;
    for (auto& entry : strikes) {
        auto& strike = entry.second;
        removed |= strike.glyphImages.erase(glyphID) != 0;
        strike.glyphIDs.erase(std::remove(strike.glyphIDs.begin(), strike.glyphIDs.end(), glyphID), strike.glyphIDs.end());
    }
    if (removed) removedGlyphs.push_back(glyphID);
    return removed;
}

bool CBDT_CBLC_Parser::replaceGlyphImage(uint16_t glyphID, const std::vector<uint8_t>& png) {
    if (!validPNG(png)) return false;
    try { detail::decodePNG(png); }
    catch (const std::exception&) { return false; }
    View view{png.data(), png.size()};
    const auto width = view.u32(16), height = view.u32(20);
    if (width > 255 || height > 255) return false;
    bool found = false;
    for (const auto& entry : strikes) {
        auto image = entry.second.glyphImages.find(glyphID);
        if (image == entry.second.glyphImages.end()) continue;
        if (image->second.imageFormat != 17 && image->second.imageFormat != 18) return false;
        found = true;
    }
    if (!found) return false;
    for (auto& entry : strikes) {
        auto it = entry.second.glyphImages.find(glyphID);
        if (it == entry.second.glyphImages.end()) continue;
        auto& image = it->second;
        image.recordData.resize(image.imageFormat == 17 ? 5 : 8);
        image.recordData[0] = height;
        image.recordData[1] = width;
        append32(image.recordData, png.size());
        image.recordData.insert(image.recordData.end(), png.begin(), png.end());
        image.data = png;
        image.width = width;
        image.height = height;
        image.length = image.recordData.size();
    }
    return true;
}
} // namespace fontmaster
