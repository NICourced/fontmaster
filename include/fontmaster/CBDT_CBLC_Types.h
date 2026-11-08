#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>

namespace fontmaster {

/**
 * Описание изображения глифа
 */
struct GlyphImage {
    uint16_t glyphID = 0;
    uint16_t imageFormat = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    std::vector<uint8_t> data;
    // Complete CBDT record, including metrics and the PNG length field.
    std::vector<uint8_t> recordData;
    uint16_t width = 0;
    uint16_t height = 0;
    int16_t bearingX = 0;
    int16_t bearingY = 0;
    uint16_t advance = 0;
};

/**
 * Описание страйка (strike)
 */
struct StrikeRecord {
    uint16_t ppem = 0;
    uint16_t resolution = 72;
    // Original 48-byte BitmapSize record, preserving line metrics and flags.
    std::vector<uint8_t> bitmapSizeTable;
    std::vector<uint16_t> glyphIDs;
    std::map<uint16_t, GlyphImage> glyphImages;
};

} // namespace fontmaster
