#include "fontmaster/TTFUtils.h"
#include <vector>
#include <stdexcept>

namespace fontmaster {
namespace utils {

class MAXPParser {
private:
    const std::vector<uint8_t>& fontData;
    uint32_t maxpOffset;
    uint16_t numGlyphs;
    uint16_t maxPoints;
    uint16_t maxContours;
    uint16_t maxCompositePoints;
    uint16_t maxCompositeContours;
    uint16_t maxZones;
    uint16_t maxTwilightPoints;
    uint16_t maxStorage;
    uint16_t maxFunctionDefs;
    uint16_t maxInstructionDefs;
    uint16_t maxStackElements;
    uint16_t maxSizeOfInstructions;
    uint16_t maxComponentElements;
    uint16_t maxComponentDepth;
    
public:
    MAXPParser(const std::vector<uint8_t>& data, uint32_t offset) 
        : fontData(data), maxpOffset(offset), numGlyphs(0), maxPoints(0), maxContours(0),
          maxCompositePoints(0), maxCompositeContours(0), maxZones(0), maxTwilightPoints(0),
          maxStorage(0), maxFunctionDefs(0), maxInstructionDefs(0), maxStackElements(0),
          maxSizeOfInstructions(0), maxComponentElements(0), maxComponentDepth(0) {}
    
    bool parse() {
        try {
            if (maxpOffset + 6 > fontData.size()) {
                return false;
            }
            
            const uint8_t* data = fontData.data() + maxpOffset;
            
            uint32_t version = readUInt32(data);
            numGlyphs = readUInt16(data + 4);
            
            if (version == 0x00010000 && maxpOffset + 32 <= fontData.size()) {
                // Version 1.0 - читаем все поля
                maxPoints = readUInt16(data + 6);
                maxContours = readUInt16(data + 8);
                maxCompositePoints = readUInt16(data + 10);
                maxCompositeContours = readUInt16(data + 12);
                maxZones = readUInt16(data + 14);
                maxTwilightPoints = readUInt16(data + 16);
                maxStorage = readUInt16(data + 18);
                maxFunctionDefs = readUInt16(data + 20);
                maxInstructionDefs = readUInt16(data + 22);
                maxStackElements = readUInt16(data + 24);
                maxSizeOfInstructions = readUInt16(data + 26);
                maxComponentElements = readUInt16(data + 28);
                maxComponentDepth = readUInt16(data + 30);
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "MAXP table parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    uint16_t getNumGlyphs() const { return numGlyphs; }
    uint16_t getMaxPoints() const { return maxPoints; }
    uint16_t getMaxContours() const { return maxContours; }
    uint16_t getMaxCompositePoints() const { return maxCompositePoints; }
    uint16_t getMaxCompositeContours() const { return maxCompositeContours; }
    uint16_t getMaxComponentDepth() const { return maxComponentDepth; }
    
private:
    uint16_t readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    
    uint32_t readUInt32(const uint8_t* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
};

} // namespace utils
} // namespace fontmaster
