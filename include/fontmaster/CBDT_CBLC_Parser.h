#ifndef CBDT_CBLC_PARSER_H
#define CBDT_CBLC_PARSER_H

#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <string>

namespace fontmaster {

class CBDT_CBLC_Parser {
public:
    struct StrikeInfo {
        uint16_t ppem;
        uint16_t resolution;
        uint32_t startGlyphIndex;
        uint32_t endGlyphIndex;
        uint32_t colorRef;
    };
    
    struct GlyphRecord {
        uint16_t glyphID;
        uint16_t width;
        uint16_t height;
        int16_t bearingX;
        int16_t bearingY;
        uint16_t advance;
        uint8_t imageFormat;
        uint32_t imageLength;
        uint32_t imageOffset;
    };
    
    struct StrikeData {
        std::vector<GlyphRecord> glyphRecords;
        uint32_t indexSubtableArrayOffset;
        uint32_t indexTablesSize;
        uint32_t numberOfIndexSubTables;
        uint32_t colorRef;
        uint16_t hori;
        uint16_t vert;
        uint16_t startGlyphIndex;
        uint16_t endGlyphIndex;
        uint16_t ppemX;
        uint16_t ppemY;
        uint8_t bitDepth;
        uint16_t flags;
    };
    
    struct GlyphImage {
        uint16_t glyphID;
        uint32_t unicodeValue;
        uint32_t strikeIndex;
        uint32_t dataOffset;
        uint32_t dataSize;
        uint16_t width;
        uint16_t height;
        int16_t bearingX;
        int16_t bearingY;
        uint16_t advance;
        uint8_t format;
        std::vector<uint8_t> bitmapData;
        
        // PNG specific data
        uint32_t pngWidth;
        uint32_t pngHeight;
        uint8_t pngColorType;
        uint8_t pngBitDepth;
        
        // TIFF specific data
        uint32_t tiffWidth;
        uint32_t tiffHeight;
        uint16_t tiffCompression;
        uint16_t tiffPhotometric;
    };

    CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData);
    
    bool parse();
    
    const std::vector<StrikeInfo>& getStrikes() const;
    const std::vector<GlyphImage>& getGlyphImages() const;
    const std::vector<StrikeData>& getStrikeData() const;

private:
    std::vector<uint8_t> fontData;
    std::vector<StrikeInfo> strikes;
    std::vector<StrikeData> strikeData;
    std::vector<GlyphImage> glyphImages;
    std::map<uint16_t, std::set<uint32_t>> glyphIDToUnicode;

    // CBLC parsing
    bool parseCBLCTable(uint32_t offset, uint32_t length);
    bool parseStrike(uint32_t offset, StrikeData& strike);
    bool parseIndexSubtable(uint32_t offset, StrikeData& strike);
    bool parseIndexFormat1(uint32_t offset, StrikeData& strike, uint16_t firstGlyph, uint16_t lastGlyph, uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat2(uint32_t offset, StrikeData& strike, uint16_t firstGlyph, uint16_t lastGlyph, uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat3(uint32_t offset, StrikeData& strike, uint16_t firstGlyph, uint16_t lastGlyph, uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat4(uint32_t offset, StrikeData& strike, uint16_t firstGlyph, uint16_t lastGlyph, uint16_t imageFormat, uint32_t imageDataOffset);
    bool parseIndexFormat5(uint32_t offset, StrikeData& strike, uint16_t firstGlyph, uint16_t lastGlyph, uint16_t imageFormat, uint32_t imageDataOffset);
    
    // CBDT parsing
    bool parseCBDTTable(uint32_t offset, uint32_t length);
    bool parseGlyphImage(uint32_t strikeIndex, uint16_t glyphID, uint32_t imageOffset,
                        uint16_t width, uint16_t height, uint32_t format);
    
    // Image format parsers
    bool parseFormat1(const uint8_t* data, GlyphImage& image);
    bool parseFormat2(const uint8_t* data, GlyphImage& image);
    bool parseFormat3(const uint8_t* data, GlyphImage& image);
    bool parseFormat4(const uint8_t* data, GlyphImage& image);
    bool parseFormat5(const uint8_t* data, GlyphImage& image);
    bool parseFormat6(const uint8_t* data, GlyphImage& image);
    bool parseFormat7(const uint8_t* data, GlyphImage& image);
    bool parseFormat8(const uint8_t* data, GlyphImage& image);
    bool parseFormat9(const uint8_t* data, GlyphImage& image);
    bool parseFormat17(const uint8_t* data, GlyphImage& image);
    bool parseFormat18(const uint8_t* data, GlyphImage& image);
    
    // PNG parsing
    bool parsePNGData(const uint8_t* data, uint32_t length, GlyphImage& image);
    bool parsePNGChunk(const uint8_t* chunkData, GlyphImage& image);
    bool parseIHDRChunk(const uint8_t* data, GlyphImage& image);
    uint32_t crc32(const uint8_t* data, uint32_t length);
    
    // TIFF parsing
    bool parseTIFFData(const uint8_t* data, uint32_t length, GlyphImage& image);
    bool parseTIFFHeader(const uint8_t* data, GlyphImage& image);
    bool parseTIFFDirectory(const uint8_t* data, uint32_t offset, GlyphImage& image);
    uint16_t readTIFFShort(const uint8_t* data, bool bigEndian);
    uint32_t readTIFFLong(const uint8_t* data, bool bigEndian);
    
    // Utility functions
    uint32_t calculateImageSize(uint16_t width, uint16_t height, uint32_t format);
    uint32_t readUInt24(const uint8_t* data);
    uint32_t readUInt32(const uint8_t* data);
    uint16_t readUInt16(const uint8_t* data);
};

} // namespace fontmaster

#endif // CBDT_CBLC_PARSER_H
