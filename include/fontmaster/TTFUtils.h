#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace fontmaster {
namespace utils {

#pragma pack(push, 1)
struct TableRecord {
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

struct TTFHeader {
    uint32_t sfntVersion;
    uint16_t numTables;
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
};
#pragma pack(pop)

class TTFReader {
private:
    const std::vector<uint8_t>& data;
    size_t pos;
    
public:
    TTFReader(const std::vector<uint8_t>& fontData) : data(fontData), pos(0) {}
    
    uint8_t readUInt8() {
        if (pos >= data.size()) throw std::runtime_error("Read beyond buffer");
        return data[pos++];
    }
    
    int8_t readInt8() {
        return static_cast<int8_t>(readUInt8());
    }
    
    uint16_t readUInt16() {
        if (pos + 2 > data.size()) throw std::runtime_error("Read beyond buffer");
        uint16_t value = (data[pos] << 8) | data[pos + 1];
        pos += 2;
        return value;
    }
    
    int16_t readInt16() {
        return static_cast<int16_t>(readUInt16());
    }
    
    uint32_t readUInt32() {
        if (pos + 4 > data.size()) throw std::runtime_error("Read beyond buffer");
        uint32_t value = (data[pos] << 24) | (data[pos + 1] << 16) | 
                        (data[pos + 2] << 8) | data[pos + 3];
        pos += 4;
        return value;
    }
    
    void seek(size_t newPos) {
        if (newPos >= data.size()) throw std::runtime_error("Seek beyond buffer");
        pos = newPos;
    }
    
    size_t tell() const { return pos; }
    
    std::vector<uint8_t> readBytes(size_t count) {
        if (pos + count > data.size()) throw std::runtime_error("Read beyond buffer");
        std::vector<uint8_t> result(data.begin() + pos, data.begin() + pos + count);
        pos += count;
        return result;
    }
    
    std::string readString(size_t length) {
        if (pos + length > data.size()) throw std::runtime_error("Read beyond buffer");
        std::string result(data.begin() + pos, data.begin() + pos + length);
        pos += length;
        return result;
    }
    
    bool checkTag(const char* tag) {
        if (pos + 4 > data.size()) return false;
        return memcmp(data.data() + pos, tag, 4) == 0;
    }
};

std::vector<TableRecord> parseTTFTables(const std::vector<uint8_t>& fontData);
bool hasTable(const std::vector<TableRecord>& tables, const std::string& tableTag);
const TableRecord* findTable(const std::vector<TableRecord>& tables, const std::string& tableTag);

}
}
