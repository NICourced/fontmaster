#include "fontmaster/TTFUtils.h"
#include <vector>
#include <map>
#include <string>
#include <iostream>

namespace fontmaster {
namespace utils {

class NAMEParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t nameOffset;
    std::map<uint16_t, std::string> glyphIDToName;
    std::vector<NameRecord> nameRecords;
    
public:
    NAMEParser(const std::vector<uint8_t>& data, uint32_t offset) 
        : fontData(data), nameOffset(offset) {}
    
    bool parse() {
        try {
            if (nameOffset + 6 > fontData.size()) {
                return false;
            }
            
            const uint8_t* data = fontData.data() + nameOffset;
            uint16_t format = readUInt16(data);
            uint16_t count = readUInt16(data + 2);
            uint16_t stringOffset = readUInt16(data + 4);
            
            if (format != 0 && format != 1) {
                std::cerr << "Unsupported NAME table format: " << format << std::endl;
                return false;
            }
            
            // Парсим все записи
            for (uint16_t i = 0; i < count; ++i) {
                uint32_t recordOffset = nameOffset + 6 + i * 12;
                if (recordOffset + 12 > fontData.size()) break;
                
                parseNameRecord(recordOffset, stringOffset);
            }
            
            std::cout << "NAME: Parsed " << nameRecords.size() << " name records" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "NAME table parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<uint16_t, std::string>& getGlyphNames() const { return glyphIDToName; }
    const std::vector<NameRecord>& getNameRecords() const { return nameRecords; }
    
    // Получить raw данные для определенного nameID
    std::vector<NameRecord> getNameRecordsByID(uint16_t nameID) const {
        std::vector<NameRecord> result;
        for (const auto& record : nameRecords) {
            if (record.nameID == nameID) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    // Получить PostScript names (nameID = 6)
    std::vector<NameRecord> getPostScriptNames() const {
        return getNameRecordsByID(6);
    }
    
private:
    struct NameRecord {
        uint16_t platformID;
        uint16_t encodingID;
        uint16_t languageID;
        uint16_t nameID;
        std::vector<uint8_t> rawData;
        
        std::string getString() const {
            return std::string(rawData.begin(), rawData.end());
        }
    };
    
    uint16_t readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    
    void parseNameRecord(uint32_t recordOffset, uint16_t stringOffset) {
        const uint8_t* record = fontData.data() + recordOffset;
        
        uint16_t platformID = readUInt16(record);
        uint16_t encodingID = readUInt16(record + 2);
        uint16_t languageID = readUInt16(record + 4);
        uint16_t nameID = readUInt16(record + 6);
        uint16_t length = readUInt16(record + 8);
        uint16_t offset = readUInt16(record + 10);
        
        uint32_t stringStart = nameOffset + stringOffset + offset;
        
        if (stringStart + length > fontData.size()) {
            std::cerr << "NAME string data out of bounds" << std::endl;
            return;
        }
        
        // Сохраняем raw данные без конвертации
        NameRecord nameRecord;
        nameRecord.platformID = platformID;
        nameRecord.encodingID = encodingID;
        nameRecord.languageID = languageID;
        nameRecord.nameID = nameID;
        nameRecord.rawData.assign(fontData.data() + stringStart, fontData.data() + stringStart + length);
        
        nameRecords.push_back(nameRecord);
        
        // Для PostScript names (nameID = 6) пытаемся создать mapping
        if (nameID == 6) {
            // ВНИМАНИЕ: NAME таблица не дает прямого mapping glyphID -> name
            // Мы можем только собрать все доступные имена
            std::string name = nameRecord.getString();
            if (!name.empty()) {
                // Используем индекс как временный glyphID (это НЕ надежно!)
                uint16_t tempGlyphID = static_cast<uint16_t>(nameRecords.size() - 1);
                glyphIDToName[tempGlyphID] = name;
            }
        }
    }
};

} // namespace utils
} // namespace fontmaster
