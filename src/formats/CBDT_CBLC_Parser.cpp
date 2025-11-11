#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "utils/CMAPParser.h"
#include "utils/NAMEParser.h"
#include "utils/POSTParser.h"
#include "utils/MAXPParser.h"
#include "utils/CFFParser.h"
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <cstring>
namespace fontmaster {

#pragma pack(push, 1)
struct CBLCHeader {
    uint32_t version;
    uint32_t numStrikes;
};

struct StrikeRecord {
    uint32_t ppem;
    uint32_t resolution;
    uint32_t startGlyphIndex;
    uint32_t endGlyphIndex;
    uint32_t bitmapSize;
    uint32_t offsetToIndexSubtable;
    uint32_t indexSubtableSize;
    uint32_t colorRef;
    uint32_t hMetricsCount;
    uint32_t vMetricsCount;
    uint32_t startGlyphID;
    uint32_t endGlyphID;
};

struct IndexSubtableHeader {
    uint16_t indexFormat;
    uint16_t imageFormat;
    uint32_t imageDataOffset;
};

struct IndexSubtable1 {
    IndexSubtableHeader header;
    uint32_t offsetArray[1]; // variable size
};

struct IndexSubtable2 {
    IndexSubtableHeader header;
    uint32_t imageSize;
    uint32_t bigGlyphMetricsOffset;
};

struct IndexSubtable3 {
    IndexSubtableHeader header;
    uint32_t offsetToOffsetArray;
};

struct IndexSubtable4 {
    IndexSubtableHeader header;
    uint32_t numGlyphs;
    uint32_t offsetToOffsetArray;
};
#pragma pack(pop)

class CBDT_CBLC_Parser {
private:
    const std::vector<uint8_t>& fontData;
    std::map<std::string, std::vector<CBDTStrike>> glyphStrikes;
    std::map<uint16_t, std::string> glyphIDToName;
    std::map<uint16_t, uint32_t> glyphIDToUnicode;
    uint32_t cbdtOffset;
    uint16_t numGlyphs;
    
public:
    CBDT_CBLC_Parser(const std::vector<uint8_t>& data) : fontData(data), numGlyphs(0) {}
    
    bool parse() {
        try {
            auto tables = utils::parseTTFTables(fontData);
            
            // 1. MAXP для количества глифов
            auto maxpTable = utils::findTable(tables, "maxp");
            if (maxpTable) {
                utils::MAXPParser maxpParser(fontData, maxpTable->offset);
                if (maxpParser.parse()) {
                    numGlyphs = maxpParser.getNumGlyphs();
                    std::cout << "Found " << numGlyphs << " glyphs in font" << std::endl;
                }
            }
            
            if (numGlyphs == 0) {
                std::cerr << "Cannot determine number of glyphs" << std::endl;
                return false;
            }
            
            // 2. POST таблица - основной источник имен
            auto postTable = utils::findTable(tables, "post");
            if (postTable) {
                utils::POSTParser postParser(fontData, postTable->offset, numGlyphs);
                if (postParser.parse()) {
                    glyphIDToName = postParser.getGlyphNames();
                    std::cout << "Loaded " << glyphIDToName.size() << " glyph names from POST table" << std::endl;
                }
            }
            
            // 3. CFF таблица - для OpenType CFF шрифтов
            if (glyphIDToName.size() < numGlyphs) {
                auto cffTable = utils::findTable(tables, "CFF ");
                if (cffTable) {
                    utils::CFFParser cffParser(fontData, cffTable->offset);
                    if (cffParser.parse()) {
                        auto cffNames = cffParser.getGlyphNames();
                        // Дополняем только отсутствующие имена
                        for (const auto& pair : cffNames) {
                            if (glyphIDToName.find(pair.first) == glyphIDToName.end() && pair.first < numGlyphs) {
                                glyphIDToName[pair.first] = pair.second;
                            }
                        }
                        std::cout << "Supplemented with " << cffNames.size() << " names from CFF table" << std::endl;
                    }
                }
            }
            
            // 4. NAME таблица - только для информации (не для mapping)
            auto nameTable = utils::findTable(tables, "name");
            if (nameTable) {
                utils::NAMEParser nameParser(fontData, nameTable->offset);
                if (nameParser.parse()) {
                    auto postScriptNames = nameParser.getPostScriptNames();
                    std::cout << "Found " << postScriptNames.size() << " PostScript names in NAME table" << std::endl;
                    // NAME таблица используется только для верификации/информации
                }
            }
            
            // 5. CMAP для Unicode информации
            auto cmapTable = utils::findTable(tables, "cmap");
            if (cmapTable) {
                utils::CMAPParser cmapParser(fontData, cmapTable->offset);
                if (cmapParser.parse()) {
                    glyphIDToUnicode = cmapParser.getGlyphToUnicodeMap();
                    std::cout << "Loaded Unicode mapping for " << glyphIDToUnicode.size() << " glyphs" << std::endl;
                }
            }
            
            // 6. Заполняем пропущенные имена
            for (uint16_t glyphID = 0; glyphID < numGlyphs; glyphID++) {
                if (glyphIDToName.find(glyphID) == glyphIDToName.end()) {
                    if (glyphIDToUnicode.find(glyphID) != glyphIDToUnicode.end()) {
                        // Создаем имя на основе Unicode
                        char name[32];
                        snprintf(name, sizeof(name), "u%04X", glyphIDToUnicode[glyphID]);
                        glyphIDToName[glyphID] = name;
                    } else {
                        // Fallback
                        glyphIDToName[glyphID] = "glyph" + std::to_string(glyphID);
                    }
                }
            }
            
            std::cout << "Final: " << glyphIDToName.size() << " glyph names for " << numGlyphs << " glyphs" << std::endl;
            
            // 7. Парсим CBDT/CBLC
            auto cblcTable = utils::findTable(tables, "CBLC");
            auto cbdtTable = utils::findTable(tables, "CBDT");
            
            if (!cblcTable || !cbdtTable) {
                std::cerr << "CBDT/CBLC tables not found" << std::endl;
                return false;
            }
            
            cbdtOffset = cbdtTable->offset;
            return parseCBLCTable(cblcTable->offset, cblcTable->length);
            
        } catch (const std::exception& e) {
            std::cerr << "CBDT/CBLC parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<std::string, std::vector<CBDTStrike>>& getGlyphStrikes() const {
        return glyphStrikes;
    }
    
    uint16_t getNumGlyphs() const { return numGlyphs; }
    
private:
    bool parseCBLCTable(uint32_t offset, uint32_t length) {
        if (offset + sizeof(CBLCHeader) > fontData.size()) {
            return false;
        }
        
        const CBLCHeader* header = reinterpret_cast<const CBLCHeader*>(
            fontData.data() + offset);
        
        if (header->version != 0x00020000) {
            std::cerr << "Unsupported CBLC version: " << std::hex << header->version << std::dec << std::endl;
            return false;
        }
        
        std::cout << "Parsing CBLC table with " << header->numStrikes << " strikes" << std::endl;
        
        // Парсим strike records
        const StrikeRecord* strikeRecords = reinterpret_cast<const StrikeRecord*>(
            fontData.data() + offset + sizeof(CBLCHeader));
        
        for (uint32_t i = 0; i < header->numStrikes; ++i) {
            if (offset + sizeof(CBLCHeader) + (i + 1) * sizeof(StrikeRecord) > fontData.size()) {
                std::cerr << "Strike record " << i << " out of bounds" << std::endl;
                break;
            }
            parseStrike(strikeRecords[i], i);
        }
        
        return true;
    }
    
    void parseStrike(const StrikeRecord& strike, uint32_t strikeIndex) {
        uint32_t subtableOffset = strike.offsetToIndexSubtable;
        
        if (subtableOffset + sizeof(IndexSubtableHeader) > fontData.size()) {
            std::cerr << "Index subtable header out of bounds" << std::endl;
            return;
        }
        
        const IndexSubtableHeader* header = reinterpret_cast<const IndexSubtableHeader*>(
            fontData.data() + subtableOffset);
        
        bool success = false;
        switch (header->indexFormat) {
            case 1: success = parseIndexSubtable1(header, strike, strikeIndex); break;
            case 2: success = parseIndexSubtable2(header, strike, strikeIndex); break;
            case 3: success = parseIndexSubtable3(header, strike, strikeIndex); break;
            case 4: success = parseIndexSubtable4(header, strike, strikeIndex); break;
            default:
                std::cerr << "Unsupported index subtable format: " << header->indexFormat << std::endl;
                break;
        }
        
        if (!success) {
            std::cerr << "Failed to parse index subtable format " << header->indexFormat << std::endl;
        }
    }
    
    bool parseIndexSubtable1(const IndexSubtableHeader* header, const StrikeRecord& strike, uint32_t strikeIndex) {
        const IndexSubtable1* subtable = reinterpret_cast<const IndexSubtable1*>(header);
        uint32_t glyphCount = strike.endGlyphID - strike.startGlyphID + 1;
        
        // Проверяем границы
        uint32_t arraySize = glyphCount * sizeof(uint32_t);
        uint32_t subtableEnd = reinterpret_cast<const uint8_t*>(subtable) - fontData.data() + sizeof(IndexSubtableHeader) + arraySize;
        if (subtableEnd > fontData.size()) {
            std::cerr << "Index subtable 1 data out of bounds" << std::endl;
            return false;
        }
        
        for (uint32_t i = 0; i < glyphCount; ++i) {
            uint32_t imageOffset = subtable->offsetArray[i];
            if (imageOffset == 0) continue;
            
            uint16_t glyphID = strike.startGlyphID + i;
            if (!parseGlyphImage(strikeIndex, glyphID, imageOffset, header->imageFormat, strike.ppem, strike.resolution)) {
                std::cerr << "Failed to parse glyph image for glyphID " << glyphID << std::endl;
            }
        }
        
        return true;
    }
    
    bool parseIndexSubtable2(const IndexSubtableHeader* header, const StrikeRecord& strike, uint32_t strikeIndex) {
        const IndexSubtable2* subtable = reinterpret_cast<const IndexSubtable2*>(header);
        uint32_t glyphCount = strike.endGlyphID - strike.startGlyphID + 1;
        uint32_t currentOffset = header->imageDataOffset;
        
        for (uint32_t i = 0; i < glyphCount; ++i) {
            uint16_t glyphID = strike.startGlyphID + i;
            if (!parseGlyphImage(strikeIndex, glyphID, currentOffset, header->imageFormat, strike.ppem, strike.resolution)) {
                std::cerr << "Failed to parse glyph image for glyphID " << glyphID << std::endl;
            }
            currentOffset += subtable->imageSize;
        }
        
        return true;
    }
    
    bool parseIndexSubtable3(const IndexSubtableHeader* header, const StrikeRecord& strike, uint32_t strikeIndex) {
        const IndexSubtable3* subtable = reinterpret_cast<const IndexSubtable3*>(header);
        uint32_t offsetArrayOffset = subtable->offsetToOffsetArray;
        uint32_t glyphCount = strike.endGlyphID - strike.startGlyphID + 1;
        
        if (offsetArrayOffset + glyphCount * sizeof(uint32_t) > fontData.size()) {
            std::cerr << "Index subtable 3 offset array out of bounds" << std::endl;
            return false;
        }
        
        const uint32_t* offsetArray = reinterpret_cast<const uint32_t*>(
            fontData.data() + offsetArrayOffset);
        
        for (uint32_t i = 0; i < glyphCount; ++i) {
            uint32_t imageOffset = offsetArray[i];
            if (imageOffset == 0) continue;
            
            uint16_t glyphID = strike.startGlyphID + i;
            if (!parseGlyphImage(strikeIndex, glyphID, imageOffset, header->imageFormat, strike.ppem, strike.resolution)) {
                std::cerr << "Failed to parse glyph image for glyphID " << glyphID << std::endl;
            }
        }
        
        return true;
    }
    
    bool parseIndexSubtable4(const IndexSubtableHeader* header, const StrikeRecord& strike, uint32_t strikeIndex) {
        const IndexSubtable4* subtable = reinterpret_cast<const IndexSubtable4*>(header);
        uint32_t offsetArrayOffset = subtable->offsetToOffsetArray;
        
        if (offsetArrayOffset + subtable->numGlyphs * sizeof(uint32_t) > fontData.size()) {
            std::cerr << "Index subtable 4 offset array out of bounds" << std::endl;
            return false;
        }
        
        const uint32_t* offsetArray = reinterpret_cast<const uint32_t*>(
            fontData.data() + offsetArrayOffset);
        
        for (uint32_t i = 0; i < subtable->numGlyphs; ++i) {
            uint32_t imageOffset = offsetArray[i];
            if (imageOffset == 0) continue;
            
            uint16_t glyphID = strike.startGlyphID + i;
            if (!parseGlyphImage(strikeIndex, glyphID, imageOffset, header->imageFormat, strike.ppem, strike.resolution)) {
                std::cerr << "Failed to parse glyph image for glyphID " << glyphID << std::endl;
            }
        }
        
        return true;
    }
    
    bool parseGlyphImage(uint32_t strikeIndex, uint16_t glyphID, uint32_t imageOffset, 
                        uint16_t imageFormat, uint32_t ppem, uint32_t resolution) {
        // CBDT данные начинаются после 4-байтового заголовка таблицы
        uint32_t absoluteOffset = cbdtOffset + imageOffset + 4;
        
        if (absoluteOffset >= fontData.size()) {
            std::cerr << "Glyph image offset out of bounds: " << absoluteOffset << std::endl;
            return false;
        }
        
        uint32_t dataSize = 0;
        const uint8_t* imageDataPtr = nullptr;
        
        if (imageFormat == 17) { // PNG
            if (absoluteOffset + 4 > fontData.size()) {
                std::cerr << "PNG header out of bounds" << std::endl;
                return false;
            }
            
            dataSize = *reinterpret_cast<const uint32_t*>(fontData.data() + absoluteOffset);
            imageDataPtr = fontData.data() + absoluteOffset + 4;
            
            if (imageDataPtr + dataSize > fontData.data() + fontData.size()) {
                std::cerr << "PNG data out of bounds" << std::endl;
                return false;
            }
            
            // Проверяем PNG signature
            if (dataSize >= 8 && imageDataPtr[0] == 0x89 && imageDataPtr[1] == 0x50 && 
                imageDataPtr[2] == 0x4E && imageDataPtr[3] == 0x47) {
                // Valid PNG
            } else {
                std::cerr << "Invalid PNG signature for glyph " << glyphID << std::endl;
            }
        } else if (imageFormat == 18) { // JPEG
            if (absoluteOffset + 4 > fontData.size()) {
                std::cerr << "JPEG header out of bounds" << std::endl;
                return false;
            }
            
            dataSize = *reinterpret_cast<const uint32_t*>(fontData.data() + absoluteOffset);
            imageDataPtr = fontData.data() + absoluteOffset + 4;
            
            if (imageDataPtr + dataSize > fontData.data() + fontData.size()) {
                std::cerr << "JPEG data out of bounds" << std::endl;
                return false;
            }
            
            // Проверяем JPEG signature
            if (dataSize >= 2 && imageDataPtr[0] == 0xFF && imageDataPtr[1] == 0xD8) {
                // Valid JPEG
            } else {
                std::cerr << "Invalid JPEG signature for glyph " << glyphID << std::endl;
            }
        } else if (imageFormat == 19) { // TIFF
            if (absoluteOffset + 4 > fontData.size()) {
                std::cerr << "TIFF header out of bounds" << std::endl;
                return false;
            }
            
            dataSize = *reinterpret_cast<const uint32_t*>(fontData.data() + absoluteOffset);
            imageDataPtr = fontData.data() + absoluteOffset + 4;
            
            if (imageDataPtr + dataSize > fontData.data() + fontData.size()) {
                std::cerr << "TIFF data out of bounds" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Unsupported image format: " << imageFormat << " for glyph " << glyphID << std::endl;
            return false;
        }
        
        // Извлекаем данные изображения
        std::vector<uint8_t> imageData(imageDataPtr, imageDataPtr + dataSize);
        
        // Создаем имя глифа
        std::string glyphName = getGlyphName(glyphID);
        
        // Сохраняем в структуру
        CBDTStrike strikeData;
        strikeData.ppem = ppem;
        strikeData.resolution = resolution;
        strikeData.imageData = imageData;
        strikeData.imageFormat = imageFormat;
        
        glyphStrikes[glyphName].push_back(strikeData);
        
        return true;
    }
    
    std::string getGlyphName(uint16_t glyphID) {
        // Пробуем найти имя из POST/NAME таблицы
        auto nameIt = glyphIDToName.find(glyphID);
        if (nameIt != glyphIDToName.end()) {
            return nameIt->second;
        }
        
        // Пробуем создать имя на основе Unicode
        auto unicodeIt = glyphIDToUnicode.find(glyphID);
        if (unicodeIt != glyphIDToUnicode.end()) {
            char name[32];
            snprintf(name, sizeof(name), "u%04X", unicodeIt->second);
            return name;
        }
        
        // Fallback - используем glyphID
        return "glyph" + std::to_string(glyphID);
    }
};

} // namespace fontmaster
