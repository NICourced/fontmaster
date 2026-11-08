#include "fontmaster/EmojiRenderer.h"
#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "../src/utils/ImageCodec.h"
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace fontmaster;
using namespace fontmaster::detail;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> static void rejects(Action action, const std::string& message) {
    try { action(); } catch (const FontException&) { return; }
    throw std::runtime_error(message);
}
static std::vector<uint8_t> bytes(const std::string& text) { return {text.begin(), text.end()}; }
static std::vector<uint8_t> base64(const std::string& text) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> result;
    uint32_t buffer = 0;
    unsigned bits = 0;
    for (char value : text) {
        if (value == '=') break;
        const auto digit = alphabet.find(value);
        require(digit != std::string::npos, "Invalid test fixture base64");
        buffer = (buffer << 6) | digit;
        bits += 6;
        if (bits >= 8) { bits -= 8; result.push_back(buffer >> bits); }
    }
    return result;
}
static uint32_t bigEndian(const uint8_t* data) {
    return uint32_t(data[0])<<24 | uint32_t(data[1])<<16 | uint32_t(data[2])<<8 | data[3];
}
static void writeBigEndian(uint8_t* data, uint32_t value) {
    for (int i = 0; i < 4; ++i) data[i] = value >> (24-i*8);
}
static std::vector<uint8_t> colrV0Fixture() {
    using Data = std::vector<uint8_t>;
    auto u16 = [](Data& data, int value) { data.push_back((uint16_t(value)>>8)&255); data.push_back(uint16_t(value)&255); };
    auto rectangle = [&](int x, int y, int width, int height) {
        Data glyph;
        for (int value : {1, x, y, x+width, y+height, 3, 0}) u16(glyph, value);
        glyph.insert(glyph.end(), {1,1,1,1}); // four on-curve points, signed 16-bit deltas
        for (int value : {x, width, 0, -width, y, 0, height, 0}) u16(glyph, value);
        while (glyph.size()%4) glyph.push_back(0);
        return glyph;
    };
    Data glyf = rectangle(0,0,100,100);
    const auto layer = rectangle(-200,-100,800,400);
    Data loca(16,0);
    writeBigEndian(loca.data()+4, glyf.size());
    writeBigEndian(loca.data()+8, glyf.size()); // glyph 1 has no base outline
    glyf.insert(glyf.end(),layer.begin(),layer.end());
    writeBigEndian(loca.data()+12,glyf.size());
    Data head(54,0), maxp(32,0), hhea(36,0), hmtx;
    head[1]=1; writeBigEndian(head.data()+12,0x5f0f3cf5); head[18]=3;head[19]=232;head[51]=1;
    maxp[1]=1;maxp[5]=3;maxp[7]=4;maxp[9]=1;maxp[15]=1;
    hhea[1]=1;hhea[4]=3;hhea[5]=32;hhea[6]=255;hhea[7]=56;hhea[35]=3;
    for (int i=0;i<3;++i) { u16(hmtx,800);u16(hmtx,0); }
    Data colr{0,0, 0,2, 0,0,0,14, 0,0,0,26, 0,1,
              0,0,0,0,0,1, 0,1,0,0,0,1, 0,2,0,0};
    Data cpal{0,0, 0,1, 0,1, 0,1, 0,0,0,14, 0,0, 0,0,255,255};
    Data header(12,0); header[1]=1;
    return fontmaster::utils::rebuildSfnt(header, {{"head",head},{"maxp",maxp},{"hhea",hhea},
        {"hmtx",hmtx},{"loca",loca},{"glyf",glyf},{"COLR",colr},{"CPAL",cpal}});
}
static std::vector<uint8_t> gzip(const std::vector<uint8_t>& input) {
    z_stream stream{};
    require(deflateInit2(&stream, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) == Z_OK, "gzip initialization");
    std::vector<uint8_t> output(compressBound(input.size()) + 32);
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = input.size();
    stream.next_out = output.data();
    stream.avail_out = output.size();
    const int result = deflate(&stream, Z_FINISH);
    const auto count = stream.total_out;
    deflateEnd(&stream);
    require(result == Z_STREAM_END, "gzip encoding");
    output.resize(count);
    return output;
}
static size_t visible(const RasterImage& image) {
    size_t result = 0;
    for (size_t i = 3; i < image.rgba.size(); i += 4) result += image.rgba[i] != 0;
    return result;
}
static void colored(const std::vector<uint8_t>& png, unsigned size, const std::string& label) {
    const auto image = decodePNG(png);
    require(image.width == size && image.height == size, label + ": incorrect output dimensions");
    require(visible(image) > size * size / 8, label + ": missing glyph pixels");
    std::set<uint32_t> colors;
    for (size_t i = 0; i < image.rgba.size(); i += 4) {
        if (image.rgba[i+3] > 128) colors.insert((image.rgba[i] << 16) | (image.rgba[i+1] << 8) | image.rgba[i+2]);
    }
    require(colors.size() > 16, label + ": missing color layers or gradients");
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected font directory and output directory");
        const std::filesystem::path fonts = argv[1], output = argv[2];
        RasterImage source{2, 1, {255,0,0,255, 0,0,255,128}};
        const auto png = encodePNG(source);
        require(decodePNG(png).rgba == source.rgba, "PNG round trip changed straight RGBA pixels");
        auto corrupt = png;
        corrupt[corrupt.size()-1] ^= 1;
        rejects([&] { decodePNG(corrupt); }, "PNG with bad IEND CRC accepted");
        corrupt = png;
        corrupt.resize(corrupt.size()-5);
        rejects([&] { decodePNG(corrupt); }, "Truncated PNG accepted");
        // Keep the chunk CRC correct so this specifically tests pixel decoding.
        corrupt = png;
        for (size_t offset = 8; offset+12 <= corrupt.size();) {
            const auto length = bigEndian(corrupt.data()+offset);
            if (std::string(reinterpret_cast<const char*>(corrupt.data()+offset+4), 4) == "IDAT") {
                corrupt[offset+8] = 0; // invalid zlib compression method
                writeBigEndian(corrupt.data()+offset+8+length, crc32(0, corrupt.data()+offset+4, length+4));
                break;
            }
            offset += length+12;
        }
        rejects([&] { decodePNG(corrupt); }, "PNG with corrupt compressed pixels accepted");
        rejects([&] { encodePNG({1,1,{}}); }, "Incorrect RGBA allocation accepted");
        rejects([&] { renderRasterImage(png, 0); }, "Zero output size accepted");
        rejects([&] { renderRasterImage(png, 4097); }, "Excessive output size accepted");
        const auto fitted = decodePNG(renderRasterImage(png, 32));
        require(fitted.width == 32 && fitted.height == 32, "Bitmap fit dimensions");
        require(fitted.rgba[3] == 0 && fitted.rgba[(16*32+5)*4] > 200, "Bitmap fit did not preserve aspect ratio/colors");
        // One red RGB pixel in each native SBIX raster encoding.
        const auto jpeg = base64("/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAb/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFQEBAQAAAAAAAAAAAAAAAAAABgf/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwCLAGVxf//Z");
        const auto tiff = base64("SUkqAAgAAAAKAAABBAABAAAAAQAAAAEBBAABAAAAAQAAAAIBAwADAAAAhgAAAAMBAwABAAAAAQAAAAYBAwABAAAAAgAAABEBBAABAAAAjAAAABUBAwABAAAAAwAAABYBBAABAAAAAQAAABcBBAABAAAAAwAAABwBAwABAAAAAQAAAAAAAAAIAAgACAD/AAA=");
        for (const auto& native : {jpeg, tiff}) {
            const auto rendered = decodePNG(renderRasterImage(native, 32));
            const size_t center = (16*32+16)*4;
            require(rendered.rgba[center] > 240 && rendered.rgba[center+1] < 10 && rendered.rgba[center+3] == 255,
                    "JPEG/TIFF renderer lost native SBIX pixels");
            auto truncated = native;
            truncated.resize(native.size()-2);
            rejects([&] { renderRasterImage(truncated, 32); }, "Truncated JPEG/TIFF accepted");
        }
        const auto svg = bytes("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><g id='glyph7'><rect x='5' y='5' width='20' height='40' fill='red'/></g><g id='glyph8'><circle cx='80' cy='80' r='10' fill='blue'/></g></svg>");
        require(validSVG(svg) && validSVG(gzip(svg)), "Valid SVG or gzip rejected");
        require(!validSVG(bytes("<svg><path></svg>")), "Malformed SVG accepted");
        const auto red = decodePNG(renderSVG(svg, 7, 64));
        const auto blue = decodePNG(renderSVG(gzip(svg), 8, 64));
        const size_t center = (32*64+32)*4;
        require(red.rgba[center] > 240 && red.rgba[center+2] < 10, "SVG glyph selection did not isolate red layer");
        require(blue.rgba[center+2] > 240 && blue.rgba[center] < 10, "Compressed SVG glyph selection failed");
        require(visible(red) > 1800 && visible(red) < 2400, "SVG ink bounds not fitted proportionally");
        const auto embedded = bytes("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 -1000 1000 1000'><g id='glyph1'><image x='0' y='-1000' width='1000' height='1000' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAAXNSR0IArs4c6QAAABFJREFUCJlj/M/A8J+BgYEBAA0FAgD+6nhnAAAAAElFTkSuQmCC'/></g></svg>");
        const auto embeddedImage = decodePNG(renderSVG(embedded, 1, 32));
        require(embeddedImage.rgba[(16*32+16)*4] > 240 && visible(embeddedImage) >= 512,
                "SVG embedded PNG was discarded because its measured ink rectangle was empty");
        const auto colrV0 = colrV0Fixture();
        for (uint16_t gid : {0,1}) {
            const auto color = decodePNG(renderFontGlyph(colrV0,gid,64));
            const size_t center = (32*64+32)*4;
            require(color.rgba[center] > 240 && color.rgba[center+1] < 10,
                    "COLRv0 empty/small base outline hid the colored layer");
            require(visible(color) >= 1984 && visible(color) <= 2112 && color.rgba[3] == 0,
                    "COLRv0 bounds clipped paint outside the base outline");
        }
        auto renderer = createEmojiRenderer();
        require(renderer != nullptr, "Renderer factory returned null");
        for (const auto* name : {"Noto-COLRv1.ttf", "NotoColorEmoji.ttf"}) {
            auto font = Font::load((fonts/name).string());
            const auto glyph = font->findGlyphName(0x1F600);
            require(!glyph.empty(), "Missing grinning face fixture");
            const auto rendered = renderGlyph(*font, glyph, 128);
            colored(rendered, 128, name);
            const auto oriented = decodePNG(rendered);
            size_t upperWhite = 0, lowerWhite = 0;
            for (unsigned y = 0; y < oriented.height; ++y) for (unsigned x = 0; x < oriented.width; ++x) {
                const auto* pixel = oriented.rgba.data() + (size_t(y) * oriented.width + x) * 4;
                if (pixel[0] > 230 && pixel[1] > 230 && pixel[2] > 230 && pixel[3] > 128)
                    (y < oriented.height / 2 ? upperWhite : lowerWhite)++;
            }
            require(lowerWhite > upperWhite + 100,
                    std::string(name) + ": raster orientation put the smiling mouth above the eyes");
            if (font->getFormat() == FontFormat::COLR_CPAL) {
                const auto outline = decodePNG(renderFontGlyph(font->getFontData(), 0, 64));
                require(visible(outline) > 100, "Plain outline glyph was not rasterized");
                const auto blank = decodePNG(renderFontGlyph(font->getFontData(), 1, 64));
                require(visible(blank) == 0 && blank.width == 64, "Empty glyph should be transparent");
            }
            std::ofstream file(output / (std::string(name) + ".png"), std::ios::binary);
            file.write(reinterpret_cast<const char*>(rendered.data()), rendered.size());
            colored(renderer->renderGlyph((fonts/name).string(), glyph, 32), 32, "Factory rendering");
            rejects([&] { renderGlyph(*font, "glyph_999999", 64); }, "Invalid glyph ID accepted");
            if (font->getFormat() == FontFormat::CBDT_CBLC) {
                require(font->replaceGlyphImage(glyph, png), "Cannot replace bitmap fixture");
                const auto edited = decodePNG(renderGlyph(*font, glyph, 32));
                require(edited.rgba[(16*32+5)*4] > 200, "Renderer ignored unsaved bitmap replacement");
            }
        }
        std::cout << "Rendering and image validation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
