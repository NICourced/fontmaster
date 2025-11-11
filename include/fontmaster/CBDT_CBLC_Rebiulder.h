#ifndef CBDT_CBLC_REBUILDER_H
#define CBDT_CBLC_REBUILDER_H

#include "fontmaster/TTFRebuilder.h"
#include <vector>
#include <map>
#include <cstdint>

namespace fontmaster {

struct StrikeRecord {
    uint32_t ppem;
    uint32_t resolution;
    std::vector<uint16_t> glyphIDs;
    std::map<uint16_t, std::vector<uint8_t>> glyphData;
};

class CBDT_CBLC_Rebuilder : public TTFRebuilder {
public:
    CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData, 
                       const std::map<uint32_t, StrikeRecord>& strikes,
                       const std::vector<uint16_t>& removedGlyphs);
    
    std::vector<uint8_t> rebuild() override;

private:
    std::map<uint32_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;

    void rebuildCBLCTable(std::vector<uint8_t>& cblcData);
    void rebuildCBDTTable(std::vector<uint8_t>& cbdtData);
    void rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    void rebuildIndexSubtable2(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    void rebuildIndexSubtable5(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
};

} // namespace fontmaster

#endif // CBDT_CBLC_REBUILDER_H
