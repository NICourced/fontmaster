#ifndef FONTMASTER_CBDT_CBLC_REBUILDER_H
#define FONTMASTER_CBDT_CBLC_REBUILDER_H

#include "fontmaster/TTFRebuilder.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace fontmaster {

class CBDT_CBLC_Rebuilder : public TTFRebuilder {
public:
    // УНИВЕРСАЛЬНЫЙ конструктор с шаблонными параметрами
    template<typename StrikesType, typename GlyphsType>
    CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData,
                       const StrikesType& strikes,
                       const GlyphsType& removedGlyphs)
        : TTFRebuilder(fontData), strikes(strikes), removedGlyphs(removedGlyphs) {
        // Прямое присвоение - парсер уже подготовил правильные типы
    }

    std::vector<uint8_t> rebuild() override;

private:
    std::vector<uint8_t> fontData;
    std::map<uint32_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;

    // Вспомогательные методы
    void rebuildCBLCTable(std::vector<uint8_t>& cblcData);
    void rebuildStrike(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    void rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    void rebuildCBDTTable(std::vector<uint8_t>& cbdtData);
    std::vector<uint8_t> createUpdatedFont(const std::vector<uint8_t>& newCBLCTable, 
                                         const std::vector<uint8_t>& newCBDTTable);
    
    void appendUInt32(std::vector<uint8_t>& data, uint32_t value);
    void appendUInt16(std::vector<uint8_t>& data, uint16_t value);
    void setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value);
};

} // namespace fontmaster

#endif // FONTMASTER_CBDT_CBLC_REBUILDER_H
