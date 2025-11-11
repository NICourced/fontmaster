#ifndef CBDT_CBLC_PARSER_H
#define CBDT_CBLC_PARSER_H

#include <vector>
#include <map>
#include <set>
#include <cstdint>

namespace fontmaster {

class CBDT_CBLC_Parser {
public:
    struct StrikeInfo {
        uint16_t ppem;
        uint16_t resolution;
        // Add other strike properties as needed
    };
    
    struct GlyphImage {
        uint16_t glyphID;
        uint32_t unicodeValue;
        uint32_t dataOffset;
        uint32_t dataSize;
        // Add image properties as needed
    };

    CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData);
    
    bool parse();
    
    const std::vector<StrikeInfo>& getStrikes() const;
    const std::vector<GlyphImage>& getGlyphImages() const;

private:
    std::vector<uint8_t> fontData;
    std::vector<StrikeInfo> strikes;
    std::vector<GlyphImage> glyphImages;
    std::map<uint16_t, std::set<uint32_t>> glyphIDToUnicode;

    bool parseCBLCTable(uint32_t offset, uint32_t length);
    bool parseCBDTTable(uint32_t offset, uint32_t length);
    bool parseGlyphImage(uint32_t strikeIndex, uint16_t glyphID, uint32_t imageOffset,
                        uint16_t width, uint16_t height, uint32_t format);
};

} // namespace fontmaster

#endif // CBDT_CBLC_PARSER_H
