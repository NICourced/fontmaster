#include "fontmaster/POSTParser.h"
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>

namespace fontmaster {
namespace utils {

POSTParser::POSTParser(const std::vector<uint8_t>& data, uint32_t offset, uint16_t glyphCount)
    : fontData(data), postOffset(offset), numGlyphs(glyphCount) {}

bool POSTParser::parse() {
    glyphNames.clear();
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
                return parseVersion1(data);
            case 0x00020000: // Version 2.0
                return parseVersion2(data);
            case 0x00025000: // Version 2.5
                return parseVersion25(data);
            case 0x00030000: // Version 3.0
                return parseVersion3(data);
            default:
                std::cerr << "Unsupported POST table version: " << std::hex << version << std::dec << std::endl;
                return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "POST table parsing error: " << e.what() << std::endl;
        return false;
    }
}

const std::map<uint16_t, std::string>& POSTParser::getGlyphNames() const {
    return glyphNames;
}

// Helper methods (not declared in header, so they're private)
uint16_t POSTParser::readUInt16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

uint32_t POSTParser::readUInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

int16_t POSTParser::readInt16(const uint8_t* data) {
    return static_cast<int16_t>(readUInt16(data));
}

int8_t POSTParser::readInt8(const uint8_t* data) {
    return static_cast<int8_t>(data[0]);
}

bool POSTParser::parseVersion1(const uint8_t* data) {
    (void)data;  // Unused parameter - Version 1.0 uses fixed standard names
    // Version 1.0: Все глифы используют стандартные имена Macintosh
    static const char* standardNames[] = {
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

    for (uint16_t glyphID = 0; glyphID < numGlyphs && glyphID < 258; glyphID++) {
        glyphNames[glyphID] = standardNames[glyphID];
    }
    return true;
}

bool POSTParser::parseVersion2(const uint8_t* data) {
    if (postOffset > fontData.size() || fontData.size() - postOffset < 34) return false;
    // Version 2.0: Массив индексов имен
    static const char* standardNames[] = {
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

    uint16_t numGlyphsInTable = readUInt16(data + 32);

    // Используем минимальное значение между количеством из MAXP и POST
    uint16_t actualNumGlyphs = std::min(numGlyphs, numGlyphsInTable);

    if (postOffset + 34 + actualNumGlyphs * 2 > fontData.size()) {
        return false;
    }

    const size_t stringsOffset = size_t(postOffset) + 34 + size_t(numGlyphsInTable) * 2;
    if (stringsOffset > fontData.size()) return false;
    std::vector<std::string> customNames;
    size_t cursor = stringsOffset;
    for (uint16_t glyphID = 0; glyphID < actualNumGlyphs; ++glyphID) {
        const uint16_t nameIndex = readUInt16(data + 34 + size_t(glyphID) * 2);
        if (nameIndex < 258) {
            glyphNames[glyphID] = standardNames[nameIndex];
            continue;
        }
        const size_t index = nameIndex - 258;
        while (customNames.size() <= index) {
            if (cursor >= fontData.size()) return false;
            const size_t length = fontData[cursor++];
            if (length > fontData.size() - cursor) return false;
            customNames.emplace_back(reinterpret_cast<const char*>(fontData.data() + cursor), length);
            cursor += length;
        }
        glyphNames[glyphID] = customNames[index];
    }

    return true;
}

bool POSTParser::parseVersion25(const uint8_t* data) {
    if (postOffset > fontData.size() || fontData.size() - postOffset < 34) return false;
    // Version 2.5: Смещения от стандартных имен
    static const char* standardNames[] = {
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

    uint16_t numGlyphsInTable = readUInt16(data + 32);
    uint16_t actualNumGlyphs = std::min(numGlyphs, numGlyphsInTable);

    if (postOffset + 34 + actualNumGlyphs > fontData.size()) {
        return false;
    }

    const int8_t* offsetArray = reinterpret_cast<const int8_t*>(data + 34);

    for (uint16_t glyphID = 0; glyphID < actualNumGlyphs; glyphID++) {
        int8_t offset = offsetArray[glyphID];
        int32_t nameIndex = glyphID + offset;

        if (nameIndex >= 0 && nameIndex < 258) {
            glyphNames[glyphID] = standardNames[nameIndex];
        } else {
            glyphNames[glyphID] = "bad_offset_" + std::to_string(nameIndex);
        }
    }

    return true;
}

bool POSTParser::parseVersion3(const uint8_t* data) {
    (void)data;  // Unused parameter - Version 3.0 generates names based on glyphID
    // Version 3.0: Нет имен глифов в таблице
    // Генерируем имена на основе glyphID
    for (uint16_t glyphID = 0; glyphID < numGlyphs; glyphID++) {
        glyphNames[glyphID] = "glyph" + std::to_string(glyphID);
    }
    return true;
}

} // namespace utils
} // namespace fontmaster
