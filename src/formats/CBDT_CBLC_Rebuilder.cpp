#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFRebuilder.h"
#include <vector>
#include <map>
#include <algorithm>

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

struct GlyphRecord {
    std::string name;
    uint16_t glyphID;
    std::vector<uint8_t> imageData;
    uint32_t offset;
    uint32_t length;
};
#pragma pack(pop)

class CBDT_CBLC_Rebuilder : public TTFRebuilder {
private:
    std::map<std::string, std::vector<CBDTStrike>> glyphStrikes;
    std::vector<std::string> removedGlyphs;
    std::map<uint32_t, StrikeRecord> strikes; // strikeIndex -> StrikeRecord
    
public:
    CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData, 
                       const std::map<std::string, std::vector<CBDTStrike>>& strikes,
                       const std::vector<std::string>& removed)
        : TTFRebuilder(fontData), glyphStrikes(strikes), removedGlyphs(removed) {}
    
    std::vector<uint8_t> rebuildFont() {
        rebuildCBDTTable();
        rebuildCBLCTable();
        return rebuild();
    }
    
private:
    void rebuildCBDTTable() {
        std::vector<uint8_t> cbdtData;
        
        // CBDT заголовок (4 нулевых байта)
        cbdtData.insert(cbdtData.end(), 4, 0);
        
        // Собираем все изображения глифов
        std::vector<GlyphRecord> glyphRecords;
        uint32_t currentOffset = 4; // После заголовка
        
        for (const auto& glyphPair : glyphStrikes) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphPair.first) != removedGlyphs.end()) {
                continue;
            }
            
            for (const auto& strike : glyphPair.second) {
                GlyphRecord record;
                record.name = glyphPair.first;
                record.imageData = strike.imageData;
                
                // Заголовок глифа (4 байта - размер данных)
                uint32_t dataSize = strike.imageData.size();
                cbdtData.insert(cbdtData.end(), 
                    reinterpret_cast<const uint8_t*>(&dataSize),
                    reinterpret_cast<const uint8_t*>(&dataSize) + 4);
                
                // Данные PNG
                cbdtData.insert(cbdtData.end(), 
                    strike.imageData.begin(), strike.imageData.end());
                
                record.offset = currentOffset;
                record.length = 4 + dataSize;
                currentOffset += record.length;
                
                // Выравнивание до 4 байт
                while (cbdtData.size() % 4 != 0) {
                    cbdtData.push_back(0);
                    currentOffset++;
                }
                
                glyphRecords.push_back(record);
            }
        }
        
        setTableData("CBDT", cbdtData);
        markTableModified("CBDT");
    }
    
    void rebuildCBLCTable() {
        std::vector<uint8_t> cblcData;
        
        // Заголовок CBLC
        CBLCHeader header;
        header.version = 0x00020000;
        header.numStrikes = 1; // Упрощенно - один strike
        
        cblcData.insert(cblcData.end(), 
            reinterpret_cast<const uint8_t*>(&header),
            reinterpret_cast<const uint8_t*>(&header) + sizeof(header));
        
        // Strike records
        StrikeRecord strike;
        strike.ppem = 64;
        strike.resolution = 72;
        strike.startGlyphIndex = 0;
        strike.endGlyphIndex = glyphStrikes.size() - removedGlyphs.size() - 1;
        strike.bitmapSize = 0; // Рассчитается позже
        strike.offsetToIndexSubtable = sizeof(CBLCHeader) + sizeof(StrikeRecord);
        strike.indexSubtableSize = 0; // Рассчитается позже
        strike.colorRef = 0;
        strike.hMetricsCount = 0;
        strike.vMetricsCount = 0;
        strike.startGlyphID = 0;
        strike.endGlyphID = glyphStrikes.size() - removedGlyphs.size() - 1;
        
        cblcData.insert(cblcData.end(),
            reinterpret_cast<const uint8_t*>(&strike),
            reinterpret_cast<const uint8_t*>(&strike) + sizeof(strike));
        
        // Index subtable (формат 1 - простой массив смещений)
        rebuildIndexSubtable1(cblcData, strike);
        
        // Обновляем размеры в strike record
        StrikeRecord* strikePtr = reinterpret_cast<StrikeRecord*>(cblcData.data() + sizeof(CBLCHeader));
        strikePtr->indexSubtableSize = cblcData.size() - strike.offsetToIndexSubtable;
        
        setTableData("CBLC", cblcData);
        markTableModified("CBLC");
    }
    
    void rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike) {
        IndexSubtableHeader subheader;
        subheader.indexFormat = 1;
        subheader.imageFormat = 17; // PNG
        subheader.imageDataOffset = 0; // CBDT данные начинаются с 0
        
        cblcData.insert(cblcData.end(),
            reinterpret_cast<const uint8_t*>(&subheader),
            reinterpret_cast<const uint8_t*>(&subheader) + sizeof(subheader));
        
        // Массив смещений (упрощенно)
        uint32_t glyphCount = strike.endGlyphID - strike.startGlyphID + 1;
        uint32_t currentOffset = 4; // После CBDT заголовка
        
        for (uint32_t i = 0; i < glyphCount; ++i) {
            // Упрощенная логика - каждый глиф имеет одинаковый размер
            cblcData.insert(cblcData.end(),
                reinterpret_cast<const uint8_t*>(&currentOffset),
                reinterpret_cast<const uint8_t*>(&currentOffset) + 4);
            
            // Предполагаем размер 1024 байта на глиф для демонстрации
            currentOffset += 1024 + 4; // данные + заголовок размера
        }
    }
};

} // namespace fontmaster
