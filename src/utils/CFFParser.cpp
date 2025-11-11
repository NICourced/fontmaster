#include "fontmaster/TTFUtils.h"
#include <vector>
#include <map>
#include <string>
#include <iostream>

namespace fontmaster {
namespace utils {

class CFFParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t cffOffset;
    std::map<uint16_t, std::string> glyphIDToName;
    
public:
    CFFParser(const std::vector<uint8_t>& data, uint32_t offset) 
        : fontData(data), cffOffset(offset) {}
    
    bool parse() {
        try {
            if (cffOffset + 4 > fontData.size()) {
                return false;
            }
            
            const uint8_t* data = fontData.data() + cffOffset;
            const uint8_t* current = data;
            
            // Парсим Header
            uint8_t major = current[0];
            uint8_t minor = current[1];
            uint8_t hdrSize = current[2];
            uint8_t offSize = current[3];
            
            if (major != 1) {
                std::cerr << "Unsupported CFF version: " << (int)major << std::endl;
                return false;
            }
            
            current += hdrSize;
            
            // Парсим Name INDEX
            std::vector<std::string> fontNames;
            if (!parseINDEX(current, fontNames)) {
                return false;
            }
            
            // Парсим Top DICT INDEX
            std::vector<std::vector<uint8_t>> topDicts;
            if (!parseDictINDEX(current, topDicts)) {
                return false;
            }
            
            // Парсим String INDEX
            std::vector<std::string> stringIndex;
            if (!parseINDEX(current, stringIndex)) {
                // String INDEX может отсутствовать
            }
            
            // Парсим Global Subr INDEX (опционально)
            std::vector<std::vector<uint8_t>> globalSubrs;
            parseDictINDEX(current, globalSubrs);
            
            // Парсим CharStrings INDEX - это глифы
            std::vector<std::vector<uint8_t>> charStrings;
            if (!parseDictINDEX(current, charStrings)) {
                return false;
            }
            
            // Создаем имена глифов
            return createGlyphNames(charStrings, stringIndex);
            
        } catch (const std::exception& e) {
            std::cerr << "CFF parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<uint16_t, std::string>& getGlyphNames() const { return glyphIDToName; }
    
private:
    bool parseINDEX(const uint8_t*& data, std::vector<std::string>& output) {
        if (data + 2 > fontData.data() + fontData.size()) return false;
        
        uint16_t count = readUInt16(data);
        data += 2;
        
        if (count == 0) {
            output.clear();
            return true;
        }
        
        uint8_t offSize = *data++;
        
        // Читаем offsets
        std::vector<uint32_t> offsets;
        for (uint16_t i = 0; i <= count; i++) {
            if (data + offSize > fontData.data() + fontData.size()) return false;
            uint32_t offset = readOffset(data, offSize);
            offsets.push_back(offset);
            data += offSize;
        }
        
        // Читаем данные
        for (uint16_t i = 0; i < count; i++) {
            uint32_t start = offsets[i] - 1;
            uint32_t end = offsets[i + 1] - 1;
            uint32_t length = end - start;
            
            if (data + start + length > fontData.data() + fontData.size()) return false;
            
            std::string str(reinterpret_cast<const char*>(data + start), length);
            output.push_back(str);
        }
        
        data += offsets[count] - 1;
        return true;
    }
    
    bool parseDictINDEX(const uint8_t*& data, std::vector<std::vector<uint8_t>>& output) {
        if (data + 2 > fontData.data() + fontData.size()) return false;
        
        uint16_t count = readUInt16(data);
        data += 2;
        
        if (count == 0) {
            output.clear();
            return true;
        }
        
        uint8_t offSize = *data++;
        
        std::vector<uint32_t> offsets;
        for (uint16_t i = 0; i <= count; i++) {
            if (data + offSize > fontData.data() + fontData.size()) return false;
            uint32_t offset = readOffset(data, offSize);
            offsets.push_back(offset);
            data += offSize;
        }
        
        for (uint16_t i = 0; i < count; i++) {
            uint32_t start = offsets[i] - 1;
            uint32_t end = offsets[i + 1] - 1;
            uint32_t length = end - start;
            
            if (data + start + length > fontData.data() + fontData.size()) return false;
            
            std::vector<uint8_t> dict(data + start, data + start + length);
            output.push_back(dict);
        }
        
        data += offsets[count] - 1;
        return true;
    }
    
    bool createGlyphNames(const std::vector<std::vector<uint8_t>>& charStrings, 
                         const std::vector<std::string>& stringIndex) {
        // Для CFF шрифтов используем стандартные имена
        // CIDFont: cid0, cid1, cid2, ...
        // Non-CID: имена из Encoding или glyph0, glyph1, ...
        
        for (uint16_t glyphID = 0; glyphID < charStrings.size(); glyphID++) {
            // Проверяем, есть ли имя в stringIndex
            if (glyphID < stringIndex.size() && !stringIndex[glyphID].empty()) {
                glyphIDToName[glyphID] = stringIndex[glyphID];
            } else {
                // Используем CID naming как fallback
                glyphIDToName[glyphID] = "cid" + std::to_string(glyphID);
            }
        }
        
        std::cout << "CFF: Created names for " << glyphIDToName.size() << " glyphs" << std::endl;
        return true;
    }
    
    uint16_t readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    
    uint32_t readOffset(const uint8_t* data, uint8_t offSize) {
        switch (offSize) {
            case 1: return data[0];
            case 2: return readUInt16(data);
            case 3: return (data[0] << 16) | (data[1] << 8) | data[2];
            case 4: return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            default: return 0;
        }
    }
};

} // namespace utils
} // namespace fontmaster
