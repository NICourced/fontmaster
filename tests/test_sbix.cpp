#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace fontmaster;
using Bytes = std::vector<uint8_t>;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
static void put16(Bytes &b, size_t p, uint16_t v) {
    b.at(p) = v >> 8;
    b.at(p + 1) = v;
}
static void put32(Bytes &b, size_t p, uint32_t v) {
    for (int n = 0; n < 4; ++n)
        b.at(p + n) = v >> (24 - 8 * n);
}
static uint32_t u32(const Bytes &b, size_t p) {
    return (uint32_t(b.at(p)) << 24) | (uint32_t(b.at(p + 1)) << 16) | (uint32_t(b.at(p + 2)) << 8) |
           b.at(p + 3);
}
static Bytes png() {
    return {137, 80, 78, 71, 13, 10, 26,  10, 0,   0,   0, 13, 73, 72, 68, 82, 0,  0,  0,   1,   0,  0,  0,
            1,   8,  6,  0,  0,  0,  31,  21, 196, 137, 0, 0,  0,  11, 73, 68, 65, 84, 120, 156, 99, 96, 0,
            2,   0,  0,  5,  0,  1,  122, 94, 171, 63,  0, 0,  0,  0,  73, 69, 78, 68, 174, 66,  96, 130};
}
static Bytes redPNG() {
    return {137, 80,  78, 71, 13, 10,  26,  10, 0,   0,   0,   13,  73,  72,  68,  82, 0, 0,
            0,   1,   0,  0,  0,  1,   8,   6,  0,   0,   0,   31,  21,  196, 137, 0,  0, 0,
            13,  73,  68, 65, 84, 120, 156, 99, 248, 207, 192, 240, 31,  0,   5,   0,  1, 255,
            137, 153, 61, 29, 0,  0,   0,   0,  73,  69,  78,  68,  174, 66,  96,  130};
}
static Bytes record(const std::string &type, const Bytes &data, int16_t x = 7, int16_t y = -9) {
    Bytes b(8);
    put16(b, 0, x);
    put16(b, 2, y);
    std::copy(type.begin(), type.end(), b.begin() + 4);
    b.insert(b.end(), data.begin(), data.end());
    return b;
}
static Bytes strike(uint16_t ppem, const std::map<unsigned, Bytes> &records) {
    Bytes b(4 + 7 * 4);
    put16(b, 0, ppem);
    put16(b, 2, 72);
    for (unsigned gid = 0; gid < 6; ++gid) {
        put32(b, 4 + gid * 4, b.size());
        auto it = records.find(gid);
        if (it != records.end())
            b.insert(b.end(), it->second.begin(), it->second.end());
    }
    put32(b, 28, b.size());
    return b;
}
static Bytes table() {
    Bytes b(16);
    put16(b, 0, 1);
    put16(b, 2, 1);
    put32(b, 4, 2);
    auto s1 = strike(
        16, {{1, record("png ", png())}, {3, record("dupe", {0, 1}, -2, 3)}, {5, record("dupe", {0, 3})}});
    auto s2 = strike(32, {{0, record("tiff", {73, 73, 42, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0})},
                          {1, record("png ", png(), 19, 21)},
                          {4, record("jpg ", {255, 216, 255, 217})}});
    put32(b, 8, 16);
    put32(b, 12, 16 + s1.size());
    b.insert(b.end(), s1.begin(), s1.end());
    b.insert(b.end(), s2.begin(), s2.end());
    return b;
}
static Bytes font(const Bytes &sbix, uint16_t count = 6) {
    Bytes head(54), maxp(6);
    put16(head, 18, 1000);
    put32(maxp, 0, 0x00005000);
    put16(maxp, 4, count);
    std::map<std::string, Bytes> tables{{"head", head}, {"maxp", maxp}, {"sbix", sbix}};
    Bytes b(60);
    put32(b, 0, 0x10000);
    put16(b, 4, 3);
    size_t n = 0;
    for (const auto &t : tables) {
        size_t p = 12 + 16 * n++;
        std::copy(t.first.begin(), t.first.end(), b.begin() + p);
        put32(b, p + 8, b.size());
        put32(b, p + 12, t.second.size());
        b.insert(b.end(), t.second.begin(), t.second.end());
        while (b.size() % 4)
            b.push_back(0);
    }
    return b;
}
static void write(const std::string &path, const Bytes &data) {
    std::ofstream s(path, std::ios::binary);
    s.write(reinterpret_cast<const char *>(data.data()), data.size());
    require(s.good(), "fixture write");
}
static Bytes sbix(const Bytes &data) {
    auto t = utils::parseTTFTables(data);
    auto r = utils::findTable(t, "sbix");
    require(r, "sbix table preserved");
    return Bytes(data.begin() + r->offset, data.begin() + r->offset + r->length);
}
static Bytes glyph(const Bytes &table, unsigned strikeIndex, unsigned gid) {
    auto p = u32(table, 8 + 4 * strikeIndex);
    auto a = u32(table, p + 4 + 4 * gid), z = u32(table, p + 8 + 4 * gid);
    return Bytes(table.begin() + p + a, table.begin() + p + z);
}
static void rejects(const std::string &path, const Bytes &bad) {
    write(path, font(bad));
    bool threw = false;
    try {
        Font::load(path);
    } catch (const FontException &) {
        threw = true;
    }
    require(threw, "malformed sbix accepted");
}
int main(int argc, char **argv) {
    try {
        std::string dir = argc > 2 ? argv[2] : argc > 1 ? argv[1] : "/tmp";
        std::filesystem::create_directories(dir);
        auto path = dir + "/sbix-synthetic.ttf";
        write(path, font(table()));
        auto f = Font::load(path);
        require(f->getFormat() == FontFormat::SBIX, "sbix handler was not registered");
        require(f->listGlyphs().size() == 5, "multi-strike discontinuous glyph listing");
        require(f->getGlyphInfo("glyph_3").image_data == png(), "duplicate image resolution");
        require(f->getGlyphInfo("glyph_5").format == "png", "duplicate chain resolution");
        require(f->getGlyphInfo("glyph_4").format == "jpg", "JPEG format preserved");
        require(f->getGlyphInfo("glyph_0").format == "tiff", "TIFF format preserved");
        const auto before = f->getFontData();
        auto corrupt = png();
        corrupt.at(45) ^= 1;
        require(!f->replaceGlyphImage("glyph_1", corrupt), "corrupt PNG replacement accepted");
        require(f->getFontData() == before, "failed edit mutated font");
        require(f->replaceGlyphImage("glyph_1", redPNG()), "valid PNG replacement failed");
        auto t = sbix(f->getFontData());
        require(glyph(t, 0, 1) == record("png ", redPNG()), "first strike origin lost");
        require(glyph(t, 1, 1) == record("png ", redPNG(), 19, 21), "second strike origin lost");
        require(glyph(t, 1, 4) == record("jpg ", {255, 216, 255, 217}), "untouched JPEG bytes changed");
        require(glyph(t, 1, 0) == record("tiff", {73, 73, 42, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0}),
                "untouched TIFF bytes changed");
        require(f->removeGlyph("glyph_1"), "remove image");
        require(f->getGlyphInfo("glyph_3").image_data == redPNG(), "removal broke duplicate");
        require(f->getGlyphInfo("glyph_5").image_data == redPNG(), "removal broke indirect duplicate");
        require(f->listGlyphs().size() == 4, "removed glyph remains listed");
        t = sbix(f->getFontData());
        require(glyph(t, 0, 1).empty() && glyph(t, 1, 1).empty(), "remove did not clear every strike");
        require(glyph(t, 0, 3) == record("png ", redPNG(), -2, 3), "materialized duplicate origin lost");
        const auto savedPath = dir + "/edited-sbix.ttf";
        require(f->save(savedPath), "sbix save");
        auto reload = Font::load(savedPath);
        require(reload->listGlyphs().size() == 4, "saved edits not persistent");
        require(reload->getGlyphInfo("glyph_5").image_data == redPNG(), "saved duplicate broken");
        uint32_t sum = 0;
        auto data = reload->getFontData();
        for (size_t p = 0; p < data.size(); p += 4)
            sum += u32(data, p);
        require(sum == 0xB1B0AFBAu, "sfnt checksum invalid");
        auto bad = table();
        put32(bad, 8, 8);
        rejects(path, bad);
        bad = table();
        put32(bad, 16 + 28, 0xffffffff);
        rejects(path, bad);
        bad = table();
        auto first = u32(bad, 20 + 4 * 3);
        put16(bad, 16 + first + 8, 3);
        rejects(path, bad);
        bad = table();
        put16(bad, 16 + first + 8, 6);
        rejects(path, bad);
        bad = table();
        put16(bad, 16 + first + 8, 2);
        rejects(path, bad);
        bad = table();
        put32(bad, 16 + 4 + 8, 0);
        rejects(path, bad);
        auto retained = reload->getFontData();
        bool threw = false;
        try {
            reload->setFontData(font(bad));
        } catch (const std::exception &) {
            threw = true;
        }
        require(threw, "setFontData accepted malformed sbix");
        require(reload->getFontData() == retained, "setFontData failure mutated state");
        Bytes wide(12 + 4 + 65536 * 4);
        put16(wide, 0, 1);
        put16(wide, 2, 1);
        put32(wide, 4, 1);
        put32(wide, 8, 12);
        put16(wide, 12, 16);
        put16(wide, 14, 72);
        for (size_t gid = 0; gid < 65535; ++gid)
            put32(wide, 16 + gid * 4, 4 + 65536 * 4);
        auto last = record("png ", png());
        wide.insert(wide.end(), last.begin(), last.end());
        put32(wide, 16 + 65535 * 4, 4 + 65536 * 4 + last.size());
        write(path, font(wide, 65535));
        auto large = Font::load(path);
        require(large->listGlyphs().size() == 1, "maximum glyph count loop failed");
        require(large->getGlyphInfo("glyph_65534").image_data == png(), "last glyph skipped");
        require(large->removeGlyph("glyph_65534"), "last glyph removal failed");
        require(large->listGlyphs().empty(), "last glyph removal not persistent");
        std::cout << "SBIX tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "SBIX test failure: " << e.what() << '\n';
        return 1;
    }
}
