#include "ImageCodec.h"
#include "fontmaster/FontMaster.h"
#include <hb.h>
#include <hb-ot.h>
#include <hb-raster.h>
#include <librsvg/rsvg.h>
#include <turbojpeg.h>
#include <tiffio.h>
#include <cairo.h>
#include <png.h>
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <cstdio>

namespace fontmaster::detail {
namespace {
constexpr uint32_t maxDimension = 8192;
constexpr size_t maxInputBytes = 128 * 1024 * 1024;
constexpr size_t maxPixelBytes = 256 * 1024 * 1024;

void checkSize(int size) {
    if (size < 1 || size > 4096) throw FontException("Render size must be between 1 and 4096 pixels");
}
void checkDimensions(uint32_t width, uint32_t height) {
    if (!width || !height || width > maxDimension || height > maxDimension ||
        uint64_t(width) * height * 4 > maxPixelBytes)
        throw FontException("Image dimensions exceed the supported allocation limit");
}
uint32_t be32(const uint8_t* data) {
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3];
}
template<class T, void (*Destroy)(T*)> using Owner = std::unique_ptr<T, decltype(Destroy)>;
void unrefHandle(RsvgHandle* handle) { g_object_unref(handle); }
std::string errorText(GError*& error, const char* fallback) {
    std::string message = error ? error->message : fallback;
    g_clear_error(&error);
    return message;
}
Owner<RsvgHandle, unrefHandle> openSVG(const std::vector<uint8_t>& document) {
    if (document.empty() || document.size() > maxInputBytes) throw FontException("Invalid SVG document size");
    GError* error = nullptr;
    // Without a base URI librsvg permits embedded data URLs, but cannot load
    // files or network resources relative to the font or the application.
    Owner<RsvgHandle, unrefHandle> handle(rsvg_handle_new_from_data(document.data(), document.size(), &error), unrefHandle);
    if (!handle) throw FontException("Invalid SVG: " + errorText(error, "parsing failed"));
    g_clear_error(&error);
    return handle;
}
Owner<cairo_surface_t, cairo_surface_destroy> surface(int size) {
    Owner<cairo_surface_t, cairo_surface_destroy> result(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size), cairo_surface_destroy);
    if (cairo_surface_status(result.get()) != CAIRO_STATUS_SUCCESS) throw FontException("Cannot allocate rendering surface");
    return result;
}
std::vector<uint8_t> surfacePNG(cairo_surface_t* surface) {
    cairo_surface_flush(surface);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) throw FontException("Image rendering failed");
    const auto width = cairo_image_surface_get_width(surface), height = cairo_image_surface_get_height(surface);
    const auto stride = cairo_image_surface_get_stride(surface);
    const auto* data = cairo_image_surface_get_data(surface);
    RasterImage image{static_cast<uint32_t>(width), static_cast<uint32_t>(height), std::vector<uint8_t>(size_t(width)*height*4)};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t pixel;
            std::memcpy(&pixel, data + y*stride + x*4, 4);
            auto* output = image.rgba.data() + (size_t(y)*width+x)*4;
            const unsigned alpha = pixel >> 24;
            output[3] = alpha;
            for (int channel = 0; channel < 3; ++channel) {
                const unsigned premultiplied = (pixel >> (16-channel*8)) & 255;
                output[channel] = alpha ? std::min(255u, (premultiplied*255 + alpha/2)/alpha) : 0;
            }
        }
    }
    return encodePNG(image);
}
std::vector<uint8_t> fitRaster(const RasterImage& image, int size) {
    checkDimensions(image.width, image.height);
    if (image.rgba.size() != size_t(image.width)*image.height*4) throw FontException("Invalid RGBA pixel buffer");
    std::vector<uint32_t> premultiplied(size_t(image.width)*image.height);
    for (size_t i = 0; i < premultiplied.size(); ++i) {
        const auto* pixel = image.rgba.data() + i*4;
        const unsigned a = pixel[3];
        premultiplied[i] = (a << 24) | (((pixel[0]*a+127)/255) << 16) |
                          (((pixel[1]*a+127)/255) << 8) | ((pixel[2]*a+127)/255);
    }
    Owner<cairo_surface_t, cairo_surface_destroy> source(cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(premultiplied.data()), CAIRO_FORMAT_ARGB32,
        image.width, image.height, image.width*4), cairo_surface_destroy);
    auto target = surface(size);
    Owner<cairo_t, cairo_destroy> context(cairo_create(target.get()), cairo_destroy);
    const double scale = double(size)/std::max(image.width, image.height);
    cairo_translate(context.get(), (size-image.width*scale)/2, (size-image.height*scale)/2);
    cairo_scale(context.get(), scale, scale);
    cairo_set_source_surface(context.get(), source.get(), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(context.get()), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(context.get()), CAIRO_EXTEND_PAD);
    cairo_rectangle(context.get(), 0, 0, image.width, image.height);
    cairo_fill(context.get());
    if (cairo_status(context.get()) != CAIRO_STATUS_SUCCESS) throw FontException("Bitmap rendering failed");
    return surfacePNG(target.get());
}
RasterImage decodeJPEG(const std::vector<uint8_t>& bytes) {
    Owner<void, tj3Destroy> decoder(tj3Init(TJINIT_DECOMPRESS), tj3Destroy);
    if (!decoder) throw FontException("Cannot allocate JPEG decoder");
    tj3Set(decoder.get(), TJPARAM_STOPONWARNING, 1);
    tj3Set(decoder.get(), TJPARAM_MAXMEMORY, 64);
    if (tj3DecompressHeader(decoder.get(), bytes.data(), bytes.size()))
        throw FontException("Invalid JPEG header: " + std::string(tj3GetErrorStr(decoder.get())));
    const int width = tj3Get(decoder.get(), TJPARAM_JPEGWIDTH);
    const int height = tj3Get(decoder.get(), TJPARAM_JPEGHEIGHT);
    checkDimensions(width, height);
    RasterImage image{uint32_t(width), uint32_t(height), std::vector<uint8_t>(size_t(width)*height*4)};
    if (tj3Decompress8(decoder.get(), bytes.data(), bytes.size(), image.rgba.data(), 0, TJPF_RGBA))
        throw FontException("Invalid JPEG pixels: " + std::string(tj3GetErrorStr(decoder.get())));
    return image;
}
struct TiffInput { const std::vector<uint8_t>& bytes; size_t offset = 0; };
tmsize_t tiffRead(thandle_t handle, void* destination, tmsize_t count) {
    auto& input = *static_cast<TiffInput*>(handle);
    if (count < 0) return -1;
    const size_t available = std::min(size_t(count), input.bytes.size()-input.offset);
    std::memcpy(destination, input.bytes.data()+input.offset, available);
    input.offset += available;
    return available;
}
tmsize_t tiffWrite(thandle_t, void*, tmsize_t) { return -1; }
toff_t tiffSeek(thandle_t handle, toff_t offset, int origin) {
    auto& input = *static_cast<TiffInput*>(handle);
    size_t position;
    if (origin == SEEK_SET) {
        if (offset > input.bytes.size()) return toff_t(-1);
        position = offset;
    } else if (origin == SEEK_CUR || origin == SEEK_END) {
        const size_t base = origin == SEEK_CUR ? input.offset : input.bytes.size();
        if (offset <= toff_t(std::numeric_limits<int64_t>::max())) {
            if (offset > input.bytes.size()-base) return toff_t(-1);
            position = base+offset;
        } else {
            const toff_t distance = ~offset+1;
            if (distance > base) return toff_t(-1);
            position = base-distance;
        }
    } else return toff_t(-1);
    input.offset = position;
    return position;
}
int tiffClose(thandle_t) { return 0; }
toff_t tiffSize(thandle_t handle) { return static_cast<TiffInput*>(handle)->bytes.size(); }
int tiffMap(thandle_t, void**, toff_t*) { return 0; }
void tiffUnmap(thandle_t, void*, toff_t) {}
int tiffMessage(TIFF*, void*, const char*, const char*, va_list) { return 1; }
RasterImage decodeTIFF(const std::vector<uint8_t>& bytes) {
    TiffInput input{bytes};
    Owner<TIFFOpenOptions, TIFFOpenOptionsFree> options(TIFFOpenOptionsAlloc(), TIFFOpenOptionsFree);
    if (!options) throw FontException("Cannot allocate TIFF decoder options");
    TIFFOpenOptionsSetMaxSingleMemAlloc(options.get(), maxPixelBytes);
    TIFFOpenOptionsSetErrorHandlerExtR(options.get(), tiffMessage, nullptr);
    TIFFOpenOptionsSetWarningHandlerExtR(options.get(), tiffMessage, nullptr);
    Owner<TIFF, TIFFClose> decoder(TIFFClientOpenExt("font image", "rm", &input, tiffRead, tiffWrite,
        tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap, options.get()), TIFFClose);
    if (!decoder) throw FontException("Invalid TIFF header");
    uint32_t width = 0, height = 0;
    if (!TIFFGetField(decoder.get(), TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(decoder.get(), TIFFTAG_IMAGELENGTH, &height)) throw FontException("Missing TIFF dimensions");
    checkDimensions(width, height);
    std::vector<uint32_t> raster(size_t(width)*height);
    if (!TIFFReadRGBAImageOriented(decoder.get(), width, height, raster.data(), ORIENTATION_TOPLEFT, 1))
        throw FontException("Invalid TIFF pixels");
    RasterImage image{width, height, std::vector<uint8_t>(size_t(width)*height*4)};
    for (size_t i = 0; i < raster.size(); ++i) {
        auto* pixel = image.rgba.data()+i*4;
        const unsigned alpha = TIFFGetA(raster[i]);
        pixel[3] = alpha;
        // TIFFReadRGBAImage returns associated (premultiplied) alpha.
        pixel[0] = alpha ? std::min(255u, (TIFFGetR(raster[i])*255u+alpha/2)/alpha) : 0;
        pixel[1] = alpha ? std::min(255u, (TIFFGetG(raster[i])*255u+alpha/2)/alpha) : 0;
        pixel[2] = alpha ? std::min(255u, (TIFFGetB(raster[i])*255u+alpha/2)/alpha) : 0;
    }
    return image;
}
} // namespace

RasterImage decodePNG(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 33 || bytes.size() > maxInputBytes || png_sig_cmp(bytes.data(), 0, 8))
        throw FontException("Invalid PNG signature or size");
    bool ended = false;
    size_t offset = 8;
    while (offset < bytes.size()) {
        if (bytes.size()-offset < 12) throw FontException("Truncated PNG chunk");
        const uint32_t length = be32(bytes.data()+offset);
        if (length > bytes.size()-offset-12) throw FontException("Truncated PNG payload");
        const auto* type = bytes.data()+offset+4;
        if (crc32(0, type, size_t(length)+4) != be32(type+4+length)) throw FontException("Invalid PNG chunk CRC");
        if (offset == 8) {
            if (length != 13 || std::memcmp(type, "IHDR", 4)) throw FontException("Missing PNG header");
            checkDimensions(be32(type+4), be32(type+8));
        }
        offset += size_t(length)+12;
        if (!std::memcmp(type, "IEND", 4)) {
            if (length || offset != bytes.size()) throw FontException("Invalid PNG end chunk");
            ended = true;
            break;
        }
    }
    if (!ended) throw FontException("Missing PNG end chunk");
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    struct Cleanup { png_image* image; ~Cleanup() { png_image_free(image); } } cleanup{&image};
    if (!png_image_begin_read_from_memory(&image, bytes.data(), bytes.size())) throw FontException("PNG header decode failed: " + std::string(image.message));
    checkDimensions(image.width, image.height);
    image.format = PNG_FORMAT_RGBA;
    RasterImage output{image.width, image.height, std::vector<uint8_t>(PNG_IMAGE_SIZE(image))};
    if (!png_image_finish_read(&image, nullptr, output.rgba.data(), 0, nullptr))
        throw FontException("PNG pixel decode failed: " + std::string(image.message));
    return output;
}
std::vector<uint8_t> encodePNG(const RasterImage& image) {
    checkDimensions(image.width, image.height);
    if (image.rgba.size() != size_t(image.width)*image.height*4) throw FontException("Invalid RGBA pixel buffer");
    png_image output{};
    output.version = PNG_IMAGE_VERSION;
    output.width = image.width;
    output.height = image.height;
    output.format = PNG_FORMAT_RGBA;
    png_alloc_size_t length = 0;
    if (!png_image_write_to_memory(&output, nullptr, &length, 0, image.rgba.data(), 0, nullptr))
        throw FontException("Cannot size PNG output");
    std::vector<uint8_t> bytes(length);
    if (!png_image_write_to_memory(&output, bytes.data(), &length, 0, image.rgba.data(), 0, nullptr))
        throw FontException("PNG encoding failed: " + std::string(output.message));
    bytes.resize(length);
    return bytes;
}
std::vector<uint8_t> renderRasterImage(const std::vector<uint8_t>& image, int size) {
    checkSize(size);
    if (image.size() >= 8 && !png_sig_cmp(image.data(), 0, 8)) return fitRaster(decodePNG(image), size);
    if (image.size() < 4 || image.size() > maxInputBytes) throw FontException("Invalid raster image size");
    const bool jpeg = image[0] == 0xff && image[1] == 0xd8;
    const bool tiff = (image[0] == 'I' && image[1] == 'I' && image[2] == 42 && !image[3]) ||
                      (image[0] == 'M' && image[1] == 'M' && !image[2] && image[3] == 42);
    if (!jpeg && !tiff) throw FontException("Unsupported raster image format");
    return fitRaster(jpeg ? decodeJPEG(image) : decodeTIFF(image), size);
}
bool validSVG(const std::vector<uint8_t>& document) {
    try { return bool(openSVG(document)); } catch (const FontException&) { return false; }
}
std::vector<uint8_t> renderSVG(const std::vector<uint8_t>& document, uint16_t gid, int size) {
    checkSize(size);
    auto handle = openSVG(document);
    const std::string fragment = "#glyph" + std::to_string(gid);
    const char* id = rsvg_handle_has_sub(handle.get(), fragment.c_str()) ? fragment.c_str() : nullptr;
    const RsvgRectangle viewport{0, 0, double(size), double(size)};
    RsvgRectangle ink{}, logical{};
    GError* error = nullptr;
    if (!rsvg_handle_get_geometry_for_layer(handle.get(), id, &viewport, &ink, &logical, &error))
        throw FontException("Cannot measure SVG glyph: " + errorText(error, "invalid geometry"));
    // librsvg may report an empty ink rectangle for an embedded raster image,
    // even though its logical rectangle and rendered pixels are nonempty.
    if (!(ink.width > 0 && ink.height > 0) && logical.width > 0 && logical.height > 0) ink = logical;
    auto target = surface(size);
    if (ink.width > 0 && ink.height > 0) {
        const double scale = size/std::max(ink.width, ink.height);
        if (!std::isfinite(scale) || !std::isfinite(ink.x) || !std::isfinite(ink.y)) throw FontException("Invalid SVG glyph bounds");
        Owner<cairo_t, cairo_destroy> context(cairo_create(target.get()), cairo_destroy);
        cairo_translate(context.get(), (size-ink.width*scale)/2-ink.x*scale, (size-ink.height*scale)/2-ink.y*scale);
        cairo_scale(context.get(), scale, scale);
        if (!rsvg_handle_render_layer(handle.get(), context.get(), id, &viewport, &error))
            throw FontException("SVG rendering failed: " + errorText(error, "render error"));
        if (cairo_status(context.get()) != CAIRO_STATUS_SUCCESS) throw FontException("SVG surface rendering failed");
    }
    return surfacePNG(target.get());
}
std::vector<uint8_t> renderFontGlyph(const std::vector<uint8_t>& bytes, uint16_t gid, int size) {
    checkSize(size);
    if (bytes.empty() || bytes.size() > std::numeric_limits<unsigned>::max()) throw FontException("Invalid font data size");
    Owner<hb_blob_t, hb_blob_destroy> blob(hb_blob_create(reinterpret_cast<const char*>(bytes.data()), bytes.size(), HB_MEMORY_MODE_READONLY, nullptr, nullptr), hb_blob_destroy);
    Owner<hb_face_t, hb_face_destroy> face(hb_face_create(blob.get(), 0), hb_face_destroy);
    if (gid >= hb_face_get_glyph_count(face.get())) throw FontException("Glyph index is outside the font");
    Owner<hb_font_t, hb_font_destroy> font(hb_font_create(face.get()), hb_font_destroy);
    hb_ot_font_set_funcs(font.get());
    const auto upem = hb_face_get_upem(face.get());
    hb_font_set_scale(font.get(), upem, upem);
    hb_glyph_extents_t glyph{};
    if (!hb_font_get_glyph_extents(font.get(), gid, &glyph) || !glyph.width || !glyph.height)
        return encodePNG({uint32_t(size), uint32_t(size), std::vector<uint8_t>(size_t(size)*size*4)});
    const double width = std::abs(double(glyph.width)), height = std::abs(double(glyph.height));
    const float scale = size/std::max(width, height);
    const float dx = (size-width*scale)/2 - std::min(double(glyph.x_bearing), double(glyph.x_bearing)+glyph.width)*scale;
    const float dy = (size-height*scale)/2 - std::min(double(glyph.y_bearing), double(glyph.y_bearing)+glyph.height)*scale;
    const hb_raster_extents_t extents{0, 0, unsigned(size), unsigned(size), 0};
    Owner<hb_raster_paint_t, hb_raster_paint_destroy> paint(hb_raster_paint_create_or_fail(), hb_raster_paint_destroy);
    if (!paint) throw FontException("Cannot allocate color rasterizer");
    hb_raster_paint_set_transform(paint.get(), scale, 0, 0, scale, dx, dy);
    hb_raster_paint_set_extents(paint.get(), &extents);
    hb_raster_paint_set_foreground(paint.get(), HB_COLOR(0, 0, 0, 255));
    Owner<hb_raster_image_t, hb_raster_image_destroy> image(nullptr, hb_raster_image_destroy);
    if (hb_raster_paint_glyph_or_fail(paint.get(), font.get(), gid)) image.reset(hb_raster_paint_render(paint.get()));
    if (!image) {
        Owner<hb_raster_draw_t, hb_raster_draw_destroy> draw(hb_raster_draw_create_or_fail(), hb_raster_draw_destroy);
        if (!draw) throw FontException("Cannot allocate outline rasterizer");
        hb_raster_draw_set_transform(draw.get(), scale, 0, 0, scale, dx, dy);
        hb_raster_draw_set_extents(draw.get(), &extents);
        if (!hb_raster_draw_glyph_or_fail(draw.get(), font.get(), gid)) throw FontException("Glyph has no renderable outline or color paint");
        image.reset(hb_raster_draw_render(draw.get()));
    }
    if (!image) throw FontException("Font rasterization failed");
    hb_raster_extents_t raster{};
    hb_raster_image_get_extents(image.get(), &raster);
    checkDimensions(raster.width, raster.height);
    const auto* buffer = hb_raster_image_get_buffer(image.get());
    if (!buffer) throw FontException("Glyph rasterizer returned no pixels");
    const bool alphaOnly = hb_raster_image_get_format(image.get()) == HB_RASTER_FORMAT_A8;
    RasterImage output{raster.width, raster.height, std::vector<uint8_t>(size_t(raster.width)*raster.height*4)};
    for (unsigned y = 0; y < raster.height; ++y) for (unsigned x = 0; x < raster.width; ++x) {
        // HarfBuzz raster rows follow the font's upward Y axis. PNG scanlines
        // start at the top, so reverse rows for both color and A8 outlines.
        const auto* pixel = buffer + size_t(raster.height - 1 - y)*raster.stride + x*(alphaOnly ? 1 : 4);
        auto* target = output.rgba.data() + (size_t(y)*raster.width+x)*4;
        const unsigned alpha = pixel[alphaOnly ? 0 : 3];
        target[3] = alpha;
        if (!alphaOnly && alpha) for (int channel = 0; channel < 3; ++channel)
            target[channel] = std::min(255u, (pixel[2-channel]*255u + alpha/2)/alpha);
    }
    return encodePNG(output);
}
} // namespace fontmaster::detail
