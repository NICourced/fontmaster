#pragma once
#include <cstdint>
#include <vector>
namespace fontmaster::detail {
struct RasterImage { uint32_t width = 0, height = 0; std::vector<uint8_t> rgba; };
// Decode and validate a complete PNG, including compressed pixels and CRCs.
RasterImage decodePNG(const std::vector<uint8_t>& png);
std::vector<uint8_t> encodePNG(const RasterImage& image);
std::vector<uint8_t> renderRasterImage(const std::vector<uint8_t>& image, int size);
std::vector<uint8_t> renderSVG(const std::vector<uint8_t>& document, uint16_t gid, int size);
std::vector<uint8_t> renderFontGlyph(const std::vector<uint8_t>& font, uint16_t gid, int size);
bool validSVG(const std::vector<uint8_t>& document);
} // namespace fontmaster::detail
