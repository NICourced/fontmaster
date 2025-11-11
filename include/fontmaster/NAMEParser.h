#ifndef NAMEPARSER_H
#define NAMEPARSER_H

#include <vector>
#include <string>
#include <cstdint>

namespace fontmaster {
namespace utils {

class NAMEParser {
public:
    struct NameRecord {
        uint16_t platformID;
        uint16_t encodingID;
        uint16_t languageID;
        uint16_t nameID;
        std::string value;
    };

    NAMEParser(const std::vector<uint8_t>& fontData);
    
    bool parse();
    const std::vector<NameRecord>& getNameRecords() const;
    std::vector<NameRecord> getNameRecordsByID(uint16_t nameID) const;
    std::vector<NameRecord> getPostScriptNames() const;

private:
    std::vector<uint8_t> fontData;
    std::vector<NameRecord> nameRecords;

    void parseNameRecord(uint32_t recordOffset, uint16_t stringOffset);
    std::string readString(const uint8_t* data, uint16_t length, uint16_t platformID, uint16_t encodingID);
    std::string readMacString(const uint8_t* data, uint16_t length);
    std::string readWindowsString(const uint8_t* data, uint16_t length, uint16_t encodingID);
    std::string readUnicodeString(const uint8_t* data, uint16_t length);
    uint16_t readUInt16(const uint8_t* data);
};

} // namespace utils
} // namespace fontmaster

#endif // NAMEPARSER_H
