#include "fontmaster/TTFUtils.h"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>

namespace fontmaster {
namespace utils {

class CMAPParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t cmapOffset;
    std::map<uint32_t, uint16_t> unicodeToGlyphID;
    std::map<uint16_t, uint32_t> glyphIDToUnicode;
    
public:
    CMAPParser(const std::vector<uint8_t>& data, uint32_t offset) 
        : fontData(data), cmapOffset(offset) {}
    
    bool parse() {
        try {
            if (cmapOffset + 4 > fontData.size()) {
                return false;
            }
            
            const uint8_t* data = fontData.data() + cmapOffset;
            uint16_t version = readUInt16(data);
            uint16_t numTables = readUInt16(data + 2);
            
            if (version != 0) {
                return false;
            }
            
            // Парсим все subtable
            for (uint16_t i = 0; i < numTables; ++i) {
                uint32_t recordOffset = cmapOffset + 4 + i * 8;
                if (recordOffset + 8 > fontData.size()) break;
                
                uint16_t platformID = readUInt16(fontData.data() + recordOffset);
                uint16_t encodingID = readUInt16(fontData.data() + recordOffset + 2);
                uint32_t subtableOffset = readUInt32(fontData.data() + recordOffset + 4);
                
                parseSubtable(cmapOffset + subtableOffset, platformID, encodingID);
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "CMAP parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<uint32_t, uint16_t>& getUnicodeToGlyphMap() const { return unicodeToGlyphID; }
    const std::map<uint16_t, uint32_t>& getGlyphToUnicodeMap() const { return glyphIDToUnicode; }
    
private:
    uint16_t readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    
    uint32_t readUInt32(const uint8_t* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
    
    int16_t readInt16(const uint8_t* data) {
        return static_cast<int16_t>(readUInt16(data));
    }
    
    void parseSubtable(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        if (offset + 2 > fontData.size()) return;
        
        uint16_t format = readUInt16(fontData.data() + offset);
        
        switch (format) {
            case 0: parseFormat0(offset, platformID, encodingID); break;
            case 2: parseFormat2(offset, platformID, encodingID); break;
            case 4: parseFormat4(offset, platformID, encodingID); break;
            case 6: parseFormat6(offset, platformID, encodingID); break;
            case 8: parseFormat8(offset, platformID, encodingID); break;
            case 10: parseFormat10(offset, platformID, encodingID); break;
            case 12: parseFormat12(offset, platformID, encodingID); break;
            case 13: parseFormat13(offset, platformID, encodingID); break;
            case 14: parseFormat14(offset, platformID, encodingID); break;
            default:
                std::cerr << "Unsupported CMAP format: " << format << std::endl;
                break;
        }
    }
    
    void parseFormat0(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 0: Byte encoding table
        if (offset + 6 + 256 > fontData.size()) return;
        
        uint16_t length = readUInt16(fontData.data() + offset + 2);
        uint16_t language = readUInt16(fontData.data() + offset + 4);
        
        const uint8_t* glyphIdArray = fontData.data() + offset + 6;
        
        for (uint32_t charCode = 0; charCode < 256; ++charCode) {
            uint16_t glyphID = glyphIdArray[charCode];
            if (glyphID != 0) {
                unicodeToGlyphID[charCode] = glyphID;
                glyphIDToUnicode[glyphID] = charCode;
            }
        }
    }
    
    void parseFormat4(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 4: Segment mapping to delta values
        if (offset + 14 > fontData.size()) return;
        
        uint16_t length = readUInt16(fontData.data() + offset + 2);
        uint16_t language = readUInt16(fontData.data() + offset + 4);
        uint16_t segCountX2 = readUInt16(fontData.data() + offset + 6);
        uint16_t segCount = segCountX2 / 2;
        
        if (offset + length > fontData.size()) return;
        
        // Вычисляем указатели на массивы
        const uint16_t* endCount = reinterpret_cast<const uint16_t*>(fontData.data() + offset + 14);
        const uint16_t* startCount = endCount + segCount + 1; // +1 для reservedPad
        const int16_t* idDelta = reinterpret_cast<const int16_t*>(startCount + segCount);
        const uint16_t* idRangeOffset = reinterpret_cast<const uint16_t*>(idDelta + segCount);
        const uint16_t* glyphIdArray = idRangeOffset + segCount;
        
        for (uint16_t i = 0; i < segCount; ++i) {
            uint16_t end = endCount[i];
            uint16_t start = startCount[i];
            int16_t delta = idDelta[i];
            uint16_t rangeOffset = idRangeOffset[i];
            
            for (uint32_t charCode = start; charCode <= end; ++charCode) {
                uint16_t glyphID = 0;
                
                if (rangeOffset == 0) {
                    glyphID = (charCode + delta) & 0xFFFF;
                } else {
                    uint32_t glyphIndex = (rangeOffset / 2) + (charCode - start) + (i - segCount);
                    const uint16_t* glyphPtr = glyphIdArray + glyphIndex;
                    
                    if (reinterpret_cast<const uint8_t*>(glyphPtr) + 2 <= fontData.data() + fontData.size()) {
                        glyphID = *glyphPtr;
                        if (glyphID != 0) {
                            glyphID = (glyphID + delta) & 0xFFFF;
                        }
                    }
                }
                
                if (glyphID != 0) {
                    unicodeToGlyphID[charCode] = glyphID;
                    glyphIDToUnicode[glyphID] = charCode;
                }
            }
        }
    }
    
    void parseFormat6(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 6: Trimmed table mapping
        if (offset + 10 > fontData.size()) return;
        
        uint16_t length = readUInt16(fontData.data() + offset + 2);
        uint16_t language = readUInt16(fontData.data() + offset + 4);
        uint16_t firstCode = readUInt16(fontData.data() + offset + 6);
        uint16_t entryCount = readUInt16(fontData.data() + offset + 8);
        
        if (offset + 10 + entryCount * 2 > fontData.size()) return;
        
        const uint16_t* glyphIdArray = reinterpret_cast<const uint16_t*>(fontData.data() + offset + 10);
        
        for (uint16_t i = 0; i < entryCount; ++i) {
            uint32_t charCode = firstCode + i;
            uint16_t glyphID = glyphIdArray[i];
            
            if (glyphID != 0) {
                unicodeToGlyphID[charCode] = glyphID;
                glyphIDToUnicode[glyphID] = charCode;
            }
        }
    }
    
    void parseFormat12(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 12: Segmented coverage
        if (offset + 16 > fontData.size()) return;
        
        // Пропускаем format и reserved
        uint32_t length = readUInt32(fontData.data() + offset + 4);
        uint32_t language = readUInt32(fontData.data() + offset + 8);
        uint32_t numGroups = readUInt32(fontData.data() + offset + 12);
        
        if (offset + 16 + numGroups * 12 > fontData.size()) return;
        
        const uint8_t* groupsData = fontData.data() + offset + 16;
        
        for (uint32_t i = 0; i < numGroups; ++i) {
            uint32_t startCharCode = readUInt32(groupsData + i * 12);
            uint32_t endCharCode = readUInt32(groupsData + i * 12 + 4);
            uint32_t startGlyphID = readUInt32(groupsData + i * 12 + 8);
            
            for (uint32_t charCode = startCharCode; charCode <= endCharCode; ++charCode) {
                uint32_t glyphID = startGlyphID + (charCode - startCharCode);
                
                unicodeToGlyphID[charCode] = static_cast<uint16_t>(glyphID);
                glyphIDToUnicode[static_cast<uint16_t>(glyphID)] = charCode;
            }
        }
    }
    
    void parseFormat13(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 13: Many-to-one range mappings
        if (offset + 16 > fontData.size()) return;
        
        uint32_t length = readUInt32(fontData.data() + offset + 4);
        uint32_t language = readUInt32(fontData.data() + offset + 8);
        uint32_t numGroups = readUInt32(fontData.data() + offset + 12);
        
        if (offset + 16 + numGroups * 12 > fontData.size()) return;
        
        const uint8_t* groupsData = fontData.data() + offset + 16;
        
        for (uint32_t i = 0; i < numGroups; ++i) {
            uint32_t startCharCode = readUInt32(groupsData + i * 12);
            uint32_t endCharCode = readUInt32(groupsData + i * 12 + 4);
            uint32_t glyphID = readUInt32(groupsData + i * 12 + 8);
            
            for (uint32_t charCode = startCharCode; charCode <= endCharCode; ++charCode) {
                unicodeToGlyphID[charCode] = static_cast<uint16_t>(glyphID);
                glyphIDToUnicode[static_cast<uint16_t>(glyphID)] = charCode;
            }
        }
    }
    
    // Остальные форматы (2, 8, 10, 14) менее распространены для эмодзи
    void parseFormat2(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 2: High-byte mapping through table
        std::cerr << "CMAP Format 2 not fully implemented" << std::endl;
    }
    
    void parseFormat8(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 8: Mixed 16-bit and 32-bit coverage
        std::cerr << "CMAP Format 8 not fully implemented" << std::endl;
    }
    
    void parseFormat10(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 10: Trimmed array
        std::cerr << "CMAP Format 10 not fully implemented" << std::endl;
    }
    
    void parseFormat14(uint32_t offset, uint16_t platformID, uint16_t encodingID) {
        // Format 14: Unicode Variation Sequences
        std::cerr << "CMAP Format 14 not fully implemented" << std::endl;
    }
};

} // namespace utils
} // namespace fontmaster
