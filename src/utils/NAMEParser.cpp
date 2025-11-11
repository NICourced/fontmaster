#include "fontmaster/NAMEParser.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {
namespace utils {

// Структура NameRecord должна быть определена в заголовочном файле
// Если её нет, добавляем её здесь
struct NameRecord {
    uint16_t platformID;
    uint16_t encodingID;
    uint16_t languageID;
    uint16_t nameID;
    std::string value;
};

NAMEParser::NAMEParser(const std::vector<uint8_t>& fontData) : fontData(fontData) {}

bool NAMEParser::parse() {
    if (fontData.size() < 6) {
        std::cerr << "NAME: Font data too small" << std::endl;
        return false;
    }

    uint16_t format = readUInt16(fontData.data());
    uint16_t count = readUInt16(fontData.data() + 2);
    uint16_t stringOffset = readUInt16(fontData.data() + 4);

    if (format != 0 && format != 1) {
        std::cerr << "NAME: Unsupported format: " << format << std::endl;
        return false;
    }

    std::cout << "NAME: Parsing " << count << " name records" << std::endl;

    for (uint16_t i = 0; i < count; ++i) {
        parseNameRecord(6 + i * 12, stringOffset);
    }

    std::cout << "NAME: Parsed " << nameRecords.size() << " name records" << std::endl;
    return true;
}

const std::vector<NAMEParser::NameRecord>& NAMEParser::getNameRecords() const { 
    return nameRecords; 
}

std::vector<NAMEParser::NameRecord> NAMEParser::getNameRecordsByID(uint16_t nameID) const {
    std::vector<NameRecord> result;
    for (const auto& record : nameRecords) {
        if (record.nameID == nameID) {
            result.push_back(record);
        }
    }
    return result;
}

std::vector<NAMEParser::NameRecord> NAMEParser::getPostScriptNames() const {
    return getNameRecordsByID(6); // Name ID 6 is PostScript name
}

void NAMEParser::parseNameRecord(uint32_t recordOffset, uint16_t stringOffset) {
    if (recordOffset + 12 > fontData.size()) {
        std::cerr << "NAME: Name record extends beyond font data" << std::endl;
        return;
    }

    NameRecord nameRecord;
    nameRecord.platformID = readUInt16(fontData.data() + recordOffset);
    nameRecord.encodingID = readUInt16(fontData.data() + recordOffset + 2);
    nameRecord.languageID = readUInt16(fontData.data() + recordOffset + 4);
    nameRecord.nameID = readUInt16(fontData.data() + recordOffset + 6);
    uint16_t length = readUInt16(fontData.data() + recordOffset + 8);
    uint16_t offset = readUInt16(fontData.data() + recordOffset + 10);

    uint32_t stringStart = stringOffset + offset;
    if (stringStart + length <= fontData.size()) {
        nameRecord.value = readString(fontData.data() + stringStart, length, 
                                    nameRecord.platformID, nameRecord.encodingID);
    } else {
        std::cerr << "NAME: String data extends beyond font data" << std::endl;
        nameRecord.value = "";
    }

    nameRecords.push_back(nameRecord);

    // For format 1, handle language tag records
    if (nameRecord.nameID == 0 && nameRecord.platformID == 1) {
        // This is a copyright notice that might contain language info
        std::cout << "NAME: Copyright: " << nameRecord.value << std::endl;
    }
}

std::string NAMEParser::readString(const uint8_t* data, uint16_t length, 
                                  uint16_t platformID, uint16_t encodingID) {
    std::string result;
    
    if (platformID == 1) { // Macintosh
        result = readMacString(data, length);
    } else if (platformID == 3) { // Windows
        result = readWindowsString(data, length, encodingID);
    } else if (platformID == 0) { // Unicode
        result = readUnicodeString(data, length);
    } else {
        // Fallback: read as ASCII
        for (uint16_t i = 0; i < length; ++i) {
            if (data[i] >= 32 && data[i] <= 126) {
                result += static_cast<char>(data[i]);
            }
        }
    }
    
    return result;
}

std::string NAMEParser::readMacString(const uint8_t* data, uint16_t length) {
    std::string result;
    for (uint16_t i = 0; i < length; ++i) {
        // Simple MacRoman to ASCII conversion
        if (data[i] >= 32 && data[i] <= 126) {
            result += static_cast<char>(data[i]);
        } else if (data[i] == 0xA9) {
            result += "(c)"; // Copyright symbol
        } else {
            result += '?';
        }
    }
    return result;
}

std::string NAMEParser::readWindowsString(const uint8_t* data, uint16_t length, uint16_t encodingID) {
    std::string result;
    
    if (encodingID == 1 || encodingID == 10) { // UCS-2 or UCS-4
        // Simple UCS-2 to UTF-8 conversion (basic)
        for (uint16_t i = 0; i + 1 < length; i += 2) {
            uint16_t codePoint = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
            if (codePoint == 0) break;
            
            if (codePoint < 0x80) {
                result += static_cast<char>(codePoint);
            } else if (codePoint < 0x800) {
                result += static_cast<char>(0xC0 | (codePoint >> 6));
                result += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (codePoint >> 12));
                result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }
    } else {
        // Fallback: read as ASCII
        for (uint16_t i = 0; i < length; ++i) {
            if (data[i] >= 32 && data[i] <= 126) {
                result += static_cast<char>(data[i]);
            }
        }
    }
    
    return result;
}

std::string NAMEParser::readUnicodeString(const uint8_t* data, uint16_t length) {
    std::string result;
    
    // Basic Unicode (UTF-16BE) to UTF-8 conversion
    for (uint16_t i = 0; i + 1 < length; i += 2) {
        uint16_t codePoint = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        if (codePoint == 0) break;
        
        if (codePoint < 0x80) {
            result += static_cast<char>(codePoint);
        } else if (codePoint < 0x800) {
            result += static_cast<char>(0xC0 | (codePoint >> 6));
            result += static_cast<char>(0x80 | (codePoint & 0x3F));
        } else {
            // Handle surrogate pairs for UTF-16
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 3 < length) {
                // High surrogate
                uint16_t highSurrogate = codePoint;
                uint16_t lowSurrogate = (static_cast<uint16_t>(data[i + 2]) << 8) | data[i + 3];
                
                if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF) {
                    uint32_t fullCodePoint = 0x10000 + ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00);
                    
                    result += static_cast<char>(0xF0 | (fullCodePoint >> 18));
                    result += static_cast<char>(0x80 | ((fullCodePoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((fullCodePoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (fullCodePoint & 0x3F));
                    
                    i += 2; // Skip the low surrogate we just processed
                    continue;
                }
            }
            
            // Regular UTF-8 encoding
            result += static_cast<char>(0xE0 | (codePoint >> 12));
            result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }
    
    return result;
}

uint16_t NAMEParser::readUInt16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

} // namespace utils
} // namespace fontmaster
