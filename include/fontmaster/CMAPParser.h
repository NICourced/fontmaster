#ifndef CMAPPARSER_H
#define CMAPPARSER_H

#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <string>

namespace fontmaster {
namespace utils {

class CMAPParser {
public:
    CMAPParser(const std::vector<uint8_t>& fontData);
    
    bool parse();
    
    uint16_t getGlyphIndex(uint32_t charCode) const;
    std::set<uint32_t> getCharCodes(uint16_t glyphIndex) const;
    const std::map<uint32_t, uint16_t>& getCharToGlyphMap() const { return charToGlyph; }
    const std::map<uint16_t, std::set<uint32_t>>& getGlyphToCharMap() const { return glyphToChar; }

private:
    std::vector<uint8_t> fontData;
    std::map<uint32_t, uint16_t> charToGlyph;
    std::map<uint16_t, std::set<uint32_t>> glyphToChar;

    void parseSubtable(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat0(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat2(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat4(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat6(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat8(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat10(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat12(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat13(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    void parseFormat14(uint32_t offset, uint16_t platformID, uint16_t encodingID);
    
    uint16_t readUInt16(const uint8_t* data);
    uint32_t readUInt32(const uint8_t* data);
    int16_t readInt16(const uint8_t* data);
};

} // namespace utils
} // namespace fontmaster

#endif // CMAPPARSER_H
