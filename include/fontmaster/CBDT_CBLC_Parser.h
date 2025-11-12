#ifndef CBDT_CBLC_PARSER_H
#define CBDT_CBLC_PARSER_H

#include "fontmaster/TTFUtils.h"
#include <vector>
#include <map>
#include <cstdint>
#include <iostream>

namespace fontmaster {

struct GlyphImage {
    uint16_t glyphID;
    uint16_t imageFormat;
    uint32_t offset;
    uint32_t length;
    uint16_t width;
    uint16_t height;
    int16_t bearingX;
    int16_t bearingY;
    uint16_t advance;
    std::vector<uint8_t> data;
};

struct StrikeRecord {
    uint16_t ppem;
    uint16_t resolution;
    std::vector<uint16_t> glyphIDs;
    std::map<uint16_t, GlyphImage> glyphImages;
};

// TIFF Tags
static const uint16_t TIFF_TAG_IMAGE_WIDTH = 256;
static const uint16_t TIFF_TAG_IMAGE_LENGTH = 257;
static const uint16_t TIFF_TAG_BITS_PER_SAMPLE = 258;
static const uint16_t TIFF_TAG_COMPRESSION = 259;
static const uint16_t TIFF_TAG_PHOTOMETRIC = 262;
static const uint16_t TIFF_TAG_STRIP_OFFSETS = 273;
static const uint16_t TIFF_TAG_ORIENTATION = 274;
static const uint16_t TIFF_TAG_SAMPLES_PER_PIXEL = 277;
static const uint16_t TIFF_TAG_ROWS_PER_STRIP = 278;
static const uint16_t TIFF_TAG_STRIP_BYTE_COUNTS = 279;
static const uint16_t TIFF_TAG_XRESOLUTION = 282;
static const uint16_t TIFF_TAG_YRESOLUTION = 283;
static const uint16_t TIFF_TAG_PLANAR_CONFIG = 284;
static const uint16_t TIFF_TAG_XPOSITION = 286;
static const uint16_t TIFF_TAG_YPOSITION = 287;
static const uint16_t TIFF_TAG_RESOLUTION_UNIT = 296;
static const uint16_t TIFF_TAG_PREDICTOR = 317;
static const uint16_t TIFF_TAG_COLORMAP = 320;
static const uint16_t TIFF_TAG_EXTRASAMPLES = 338;

// TIFF Data Types
static const uint16_t TIFF_TYPE_BYTE = 1;
static const uint16_t TIFF_TYPE_ASCII = 2;
static const uint16_t TIFF_TYPE_SHORT = 3;
static const uint16_t TIFF_TYPE_LONG = 4;
static const uint16_t TIFF_TYPE_RATIONAL = 5;

// Compression Types
static const uint16_t TIFF_COMPRESSION_NONE = 1;
static const uint16_t TIFF_COMPRESSION_CCITT_GROUP3 = 2;
static const uint16_t TIFF_COMPRESSION_CCITT_GROUP4 = 3;

// Resolution Units
static const uint16_t TIFF_RESUNIT_NONE = 1;
static const uint16_t TIFF_RESUNIT_INCH = 2;
static const uint16_t TIFF_RESUNIT_CM = 3;

// Orientation Values
static const uint16_t TIFF_ORIENTATION_TOPLEFT = 1;
static const uint16_t TIFF_ORIENTATION_TOPRIGHT = 2;
static const uint16_t TIFF_ORIENTATION_BOTRIGHT = 3;
static const uint16_t TIFF_ORIENTATION_BOTLEFT = 4;
static const uint16_t TIFF_ORIENTATION_LEFTTOP = 5;
static const uint16_t TIFF_ORIENTATION_RIGHTTOP = 6;
static const uint16_t TIFF_ORIENTATION_RIGHTBOT = 7;
static const uint16_t TIFF_ORIENTATION_LEFTBOT = 8;

class CBDT_CBLC_Parser {
public:
    CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData);
    
    bool parse();
    const std::map<uint32_t, StrikeRecord>& getStrikes() const { return strikes; }
    const std::vector<uint16_t>& getRemovedGlyphs() const { return removedGlyphs; }
    const std::vector<uint8_t>& getFontData() const { return fontData; }
private:
    std::vector<uint8_t> fontData;
    std::map<uint32_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;
    
    bool parseCBLCTable(uint32_t offset, uint32_t length);
    bool parseCBDTTable(uint32_t offset, uint32_t length);
    bool parseCMAPTable(uint32_t offset, uint32_t length);
    
    bool parseStrike(uint32_t offset, uint32_t strikeIndex);
    bool parseIndexSubtable(uint32_t offset, StrikeRecord& strike);
    bool parseIndexFormat1(uint32_t offset, StrikeRecord& strike, 
                          uint16_t firstGlyph, uint16_t lastGlyph,
                          uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat2(uint32_t offset, StrikeRecord& strike,
                          uint16_t firstGlyph, uint16_t lastGlyph,
                          uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat5(uint32_t offset, StrikeRecord& strike,
                          uint16_t firstGlyph, uint16_t lastGlyph,
                          uint16_t imageFormat, uint32_t imageDataOffset);
    
    bool extractGlyphImageData(uint32_t imageOffset, GlyphImage& image);
    bool extractBitmapData(const uint8_t* data, GlyphImage& image);
    bool extractPNGData(const uint8_t* data, GlyphImage& image);
    bool extractJPEGData(const uint8_t* data, GlyphImage& image);
    bool extractTIFFData(const uint8_t* data, GlyphImage& image);
    bool parseTIFFDirectory(const uint8_t* data, uint32_t offset, GlyphImage& image, bool bigEndian);
    
    uint32_t calculateBitmapSize(uint16_t width, uint16_t height, uint16_t format);
    
    uint32_t readUInt32(const uint8_t* data);
    uint16_t readUInt16(const uint8_t* data);
    uint16_t readUInt16(const uint8_t* data, bool bigEndian);
    uint32_t readUInt32(const uint8_t* data, bool bigEndian);
    uint16_t readTIFFShort(const uint8_t* data, bool bigEndian);
    uint32_t readTIFFLong(const uint8_t* data, bool bigEndian);
};

} // namespace fontmaster

#endif // CBDT_CBLC_PARSER_H
