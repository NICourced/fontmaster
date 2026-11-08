#include "SVGTable.h"
#include "fontmaster/FontMaster.h"
#include "utils/ImageCodec.h"
#include <limits>
#include <string>
#include <array>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <zlib.h>

namespace fontmaster::detail {
namespace {
[[noreturn]] void invalid(const std::string& reason) { throw FontFormatException("SVG", reason); }
uint16_t u16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 2) invalid("Truncated uint16");
    return (uint16_t(data[offset]) << 8) | data[offset + 1];
}
uint32_t u32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 4) invalid("Truncated uint32");
    return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) |
           (uint32_t(data[offset + 2]) << 8) | data[offset + 3];
}
void put16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
    data.at(offset) = value >> 8; data.at(offset + 1) = value;
}
void put32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) data.at(offset + i) = value >> (24 - i * 8);
}
std::string base64(const std::vector<uint8_t>& data) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t i = 0; i < data.size(); i += 3) {
        const uint32_t value = (uint32_t(data[i]) << 16) |
            (i + 1 < data.size() ? uint32_t(data[i + 1]) << 8 : 0) |
            (i + 2 < data.size() ? data[i + 2] : 0);
        result += alphabet[(value >> 18) & 63]; result += alphabet[(value >> 12) & 63];
        result += i + 1 < data.size() ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < data.size() ? alphabet[value & 63] : '=';
    }
    return result;
}
std::vector<uint8_t> xmlBytes(const std::vector<uint8_t>& image) {
    if (image.size() < 2 || image[0] != 0x1F || image[1] != 0x8B) return image;
    constexpr size_t limit = 128 * 1024 * 1024;
    if (image.size() > limit) invalid("Compressed SVG document too large");
    z_stream stream{};
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) invalid("Cannot decode compressed SVG");
    struct End { z_stream* stream; ~End() { inflateEnd(stream); } } end{&stream};
    stream.next_in = const_cast<Bytef*>(image.data());
    stream.avail_in = image.size();
    std::vector<uint8_t> result;
    std::array<uint8_t, 16384> buffer;
    int status;
    do {
        stream.next_out = buffer.data(); stream.avail_out = buffer.size();
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) invalid("Invalid compressed SVG");
        const size_t size = buffer.size() - stream.avail_out;
        if (size > limit - result.size()) invalid("Decompressed SVG document too large");
        result.insert(result.end(), buffer.begin(), buffer.begin() + size);
    } while (status != Z_STREAM_END);
    if (stream.avail_in) invalid("Trailing compressed SVG data");
    return result;
}
bool hasID(xmlNode* node, const std::string& id) {
    for (; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            std::unique_ptr<xmlChar, decltype(xmlFree)> value(xmlGetProp(node, BAD_CAST "id"), xmlFree);
            if (value && id == reinterpret_cast<const char*>(value.get())) return true;
        }
        if (hasID(node->children, id)) return true;
    }
    return false;
}
std::vector<uint8_t> normalizeSVG(const std::vector<uint8_t>& image, uint16_t gid) {
    const auto xml = xmlBytes(image);
    if (xml.size() > size_t(std::numeric_limits<int>::max())) invalid("SVG document too large");
    std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> doc(xmlReadMemory(
        reinterpret_cast<const char*>(xml.data()), xml.size(), nullptr, nullptr,
        XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING), xmlFreeDoc);
    if (!doc) invalid("Cannot parse SVG replacement");
    auto* root = xmlDocGetRootElement(doc.get());
    const auto id = "glyph" + std::to_string(gid);
    if (hasID(root, id)) return image;
    if (!root || !xmlStrEqual(root->name, BAD_CAST "svg")) invalid("Missing SVG root");
    auto* group = xmlNewNode(root->ns, BAD_CAST "g");
    if (!group) invalid("Cannot allocate SVG group");
    xmlNewProp(group, BAD_CAST "id", BAD_CAST id.c_str());
    while (root->children) {
        auto* child = root->children;
        xmlUnlinkNode(child); xmlAddChild(group, child);
    }
    xmlAddChild(root, group);
    xmlChar* output = nullptr;
    int length = 0;
    xmlDocDumpMemoryEnc(doc.get(), &output, &length, "UTF-8");
    std::unique_ptr<xmlChar, decltype(xmlFree)> owned(output, xmlFree);
    if (!output || length < 1) invalid("Cannot serialize SVG replacement");
    return {output, output + length};
}
} // namespace

SVGTable::SVGTable(const std::vector<uint8_t>& table, uint16_t glyphCount) {
    if (table.size() < 10 || u16(table, 0) != 0 || u32(table, 6) != 0)
        invalid("Invalid SVG table header");
    const size_t list = u32(table, 2);
    if (list < 10 || list > table.size() || table.size() - list < 2)
        invalid("SVG document list outside table");
    const size_t count = u16(table, list);
    if (count > (table.size() - list - 2) / 12) invalid("Truncated SVG document records");
    const size_t recordsEnd = 2 + count * 12;
    std::map<std::pair<uint32_t, uint32_t>, Document> documents;
    uint32_t previousEnd = 0;
    for (size_t index = 0; index < count; ++index) {
        const size_t record = list + 2 + index * 12;
        const uint16_t first = u16(table, record), last = u16(table, record + 2);
        if (first > last || last >= glyphCount || (index && first <= previousEnd))
            invalid("Invalid, overlapping or unsorted SVG glyph range");
        previousEnd = last;
        const uint32_t offset = u32(table, record + 4), length = u32(table, record + 8);
        if (offset < recordsEnd || offset > table.size() - list || !length ||
            length > table.size() - list - offset)
            invalid("SVG document outside table");
        const auto key = std::make_pair(offset, length);
        auto found = documents.find(key);
        if (found == documents.end()) {
            auto document = std::make_shared<const std::vector<uint8_t>>(
                table.begin() + list + offset, table.begin() + list + offset + length);
            if (!validSVG(*document)) invalid("Malformed SVG document");
            found = documents.emplace(key, std::move(document)).first;
        }
        for (uint32_t gid = first; gid <= last; ++gid) glyphs.emplace(static_cast<uint16_t>(gid), found->second);
    }
}

std::vector<uint8_t> SVGTable::build() const {
    struct Range { uint16_t first, last; Document document; };
    std::vector<Range> ranges;
    for (const auto& entry : glyphs) {
        if (!entry.second || entry.second->empty()) invalid("Empty SVG document");
        if (!ranges.empty() && uint32_t(ranges.back().last) + 1 == entry.first && ranges.back().document == entry.second)
            ranges.back().last = entry.first;
        else ranges.push_back({entry.first, entry.first, entry.second});
    }
    if (ranges.size() > std::numeric_limits<uint16_t>::max()) invalid("Too many SVG document ranges");
    std::vector<uint8_t> result(12 + ranges.size() * 12, 0);
    put32(result, 2, 10); put16(result, 10, ranges.size());
    std::map<Document, uint32_t, std::owner_less<Document>> offsets;
    for (size_t index = 0; index < ranges.size(); ++index) {
        const auto& range = ranges[index];
        auto found = offsets.find(range.document);
        if (found == offsets.end()) {
            if (result.size() - 10 > UINT32_MAX || range.document->size() > UINT32_MAX ||
                range.document->size() > UINT32_MAX - (result.size() - 10)) invalid("SVG table too large");
            found = offsets.emplace(range.document, result.size() - 10).first;
            result.insert(result.end(), range.document->begin(), range.document->end());
        }
        const size_t record = 12 + index * 12;
        put16(result, record, range.first); put16(result, record + 2, range.last);
        put32(result, record + 4, found->second); put32(result, record + 8, range.document->size());
    }
    return result;
}

bool SVGTable::replace(uint16_t gid, const std::vector<uint8_t>& image, uint16_t upem) {
    try {
        if (validSVG(image)) {
            glyphs[gid] = std::make_shared<const std::vector<uint8_t>>(normalizeSVG(image, gid));
            return true;
        }
        const auto png = decodePNG(image);
        if (!png.width || !png.height || !upem) return false;
        const auto em = std::to_string(upem);
        const std::string document = "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 -" + em + " " + em + " " + em + "'><g id='glyph" + std::to_string(gid) + "'><image x='0' y='-" + em + "' width='" + em + "' height='" + em + "' preserveAspectRatio='xMidYMid meet' xlink:href='data:image/png;base64," + base64(image) + "'/></g></svg>";
        glyphs[gid] = std::make_shared<const std::vector<uint8_t>>(document.begin(), document.end());
        return true;
    } catch (const std::exception&) { return false; }
}
} // namespace fontmaster::detail
