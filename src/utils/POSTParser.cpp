#include "fontmaster/TTFUtils.h"
#include <vector>
#include <map>
#include <string>
#include <stdexcept>

namespace fontmaster {
namespace utils {

class POSTParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t postOffset;
    std::map<uint16_t, std::string> glyphNames;
    std::vector<std::string> standardNames;
    uint16_t numGlyphs;
    
    // Стандартные имена глифов Macintosh (258 names)
    const char* macStandardNames[258] = {
        ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl", "numbersign",
        "dollar", "percent", "ampersand", "quotesingle", "parenleft", "parenright", "asterisk",
        "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four",
        "five", "six", "seven", "eight", "nine", "colon", "semicolon", "less", "equal", "greater",
        "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
        "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash",
        "bracketright", "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f", "g",
        "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y",
        "z", "braceleft", "bar", "braceright", "asciitilde", "Adieresis", "Aring", "Ccedilla",
        "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex", "adieresis",
        "atilde", "aring", "ccedilla", "eacute", "egrave", "ecircumflex", "edieresis", "iacute",
        "igrave", "icircumflex", "idieresis", "ntilde", "oacute", "ograve", "ocircumflex", "odieresis",
        "otilde", "uacute", "ugrave", "ucircumflex", "udieresis", "dagger", "degree", "cent", "sterling",
        "section", "bullet", "paragraph", "germandbls", "registered", "copyright", "trademark", "acute",
        "dieresis", "notequal", "AE", "Oslash", "infinity", "plusminus", "lessequal", "greaterequal",
        "yen", "mu", "partialdiff", "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine",
        "Omega", "ae", "oslash", "questiondown", "exclamdown", "logicalnot", "radical", "florin",
        "approxequal", "Delta", "guillemotleft", "guillemotright", "ellipsis", "nonbreakingspace",
        "Agrave", "Atilde", "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright",
        "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis", "fraction", "currency",
        "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl", "periodcentered", "quotesinglbase",
        "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave",
        "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve",
        "Uacute", "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve", "dotaccent",
        "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron", "scaron",
        "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute", "yacute", "Thorn", "thorn", "minus",
        "multiply", "onesuperior", "twosuperior", "threesuperior", "onehalf", "onequarter", "threequarters",
        "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla", "scedilla", "Cacute", "cacute", "Ccaron",
        "ccaron", "dcroat"
    };
    
public:
    POSTParser(const std::vector<uint8_t>& data, uint32_t offset, uint16_t glyphCount) 
        : fontData(data), postOffset(offset), numGlyphs(glyphCount) {
        initializeStandardNames();
    }
    
    bool parse() {
        try {
            if (postOffset + 32 > fontData.size()) {
                return false;
            }
            
            const uint8_t* data = fontData.data() + postOffset;
            
            uint32_t version = readUInt32(data);
            // Пропускаем остальные поля заголовка, они не нужны для имен глифов
            
            // Парсим в зависимости от версии
            switch (version) {
                case 0x00010000: // Version 1.0
                    return parseVersion1();
                case 0x00020000: // Version 2.0
                    return parseVersion2(data);
                case 0x00025000: // Version 2.5
                    return parseVersion25(data);
                case 0x00030000: // Version 3.0
                    return parseVersion3();
                default:
                    std::cerr << "Unsupported POST table version: " << std::hex << version << std::dec << std::endl;
                    return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "POST table parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<uint16_t, std::string>& getGlyphNames() const { return glyphNames; }
    
private:
    void initializeStandardNames() {
        for (int i = 0; i < 258; i++) {
            standardNames.push_back(macStandardNames[i]);
        }
    }
    
    uint16_t readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    
    uint32_t readUInt32(const uint8_t* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
    
    int16_t readInt16(const uint8_t* data) {
        return static_cast<int16_t>(readUInt16(data));
    }
    
    int8_t readInt8(const uint8_t* data) {
        return static_cast<int8_t>(data[0]);
    }
    
    bool parseVersion1() {
        // Version 1.0: Все глифы используют стандартные имена Macintosh
        for (uint16_t glyphID = 0; glyphID < numGlyphs && glyphID < standardNames.size(); glyphID++) {
            glyphNames[glyphID] = standardNames[glyphID];
        }
        return true;
    }
    
    bool parseVersion2(const uint8_t* data) {
        // Version 2.0: Массив индексов имен
        uint16_t numGlyphsInTable = readUInt16(data + 32);
        
        // Используем минимальное значение между количеством из MAXP и POST
        uint16_t actualNumGlyphs = std::min(numGlyphs, numGlyphsInTable);
        
        if (postOffset + 34 + actualNumGlyphs * 2 > fontData.size()) {
            return false;
        }
        
        const uint16_t* glyphNameIndex = reinterpret_cast<const uint16_t*>(data + 32);
        
        // Вычисляем смещение до строковых данных
        uint16_t stringDataOffset = 34 + numGlyphsInTable * 2;
        
        for (uint16_t glyphID = 0; glyphID < actualNumGlyphs; glyphID++) {
            uint16_t nameIndex = glyphNameIndex[glyphID];
            
            if (nameIndex < 258) {
                // Стандартное имя
                if (nameIndex < standardNames.size()) {
                    glyphNames[glyphID] = standardNames[nameIndex];
                } else {
                    glyphNames[glyphID] = "bad_index_" + std::to_string(nameIndex);
                }
            } else {
                // Кастомное имя в строковых данных
                uint32_t customNameOffset = nameIndex - 258;
                uint32_t stringTableOffset = stringDataOffset + customNameOffset;
                
                if (stringTableOffset + 1 > fontData.size()) {
                    glyphNames[glyphID] = "invalid_offset_" + std::to_string(nameIndex);
                    continue;
                }
                
                // Читаем длину строки
                uint8_t nameLength = fontData[postOffset + stringTableOffset];
                
                if (stringTableOffset + 1 + nameLength > fontData.size()) {
                    glyphNames[glyphID] = "invalid_length_" + std::to_string(nameIndex);
                    continue;
                }
                
                // Читаем саму строку
                const uint8_t* nameData = fontData.data() + postOffset + stringTableOffset + 1;
                std::string name(reinterpret_cast<const char*>(nameData), nameLength);
                
                glyphNames[glyphID] = name;
            }
        }
        
        return true;
    }
    
    bool parseVersion25(const uint8_t* data) {
        // Version 2.5: Смещения от стандартных имен
        uint16_t numGlyphsInTable = readUInt16(data + 32);
        uint16_t actualNumGlyphs = std::min(numGlyphs, numGlyphsInTable);
        
        if (postOffset + 34 + actualNumGlyphs > fontData.size()) {
            return false;
        }
        
        const int8_t* offsetArray = reinterpret_cast<const int8_t*>(data + 32);
        
        for (uint16_t glyphID = 0; glyphID < actualNumGlyphs; glyphID++) {
            int8_t offset = offsetArray[glyphID];
            int32_t nameIndex = glyphID + offset;
            
            if (nameIndex >= 0 && nameIndex < static_cast<int32_t>(standardNames.size())) {
                glyphNames[glyphID] = standardNames[nameIndex];
            } else {
                glyphNames[glyphID] = "bad_offset_" + std::to_string(nameIndex);
            }
        }
        
        return true;
    }
    
    bool parseVersion3() {
        // Version 3.0: Нет имен глифов в таблице
        // Генерируем имена на основе glyphID
        for (uint16_t glyphID = 0; glyphID < numGlyphs; glyphID++) {
            glyphNames[glyphID] = "glyph" + std::to_string(glyphID);
        }
        return true;
    }
};

} // namespace utils
} // namespace fontmaster
