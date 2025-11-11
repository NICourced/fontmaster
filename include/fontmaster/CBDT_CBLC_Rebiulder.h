#ifndef CBDT_CBLC_REBUILDER_H
#define CBDT_CBLC_REBUILDER_H

#include "fontmaster/TTFRebuilder.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include <vector>
#include <map>

namespace fontmaster {

class CBDT_CBLC_Rebuilder : public TTFRebuilder {
public:
    CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData, 
                       const std::map<uint32_t, StrikeRecord>& strikes,
                       const std::vector<uint16_t>& removedGlyphs);
    virtual ~CBDT_CBLC_Rebuilder() = default;
    
    std::vector<uint8_t> rebuild() override;
    
private:
    std::map<uint32_t, StrikeRecord> strikes;
    std::vector<uint16_t> removedGlyphs;
    
    void rebuildCBLCTable(std::vector<uint8_t>& cblcData);
    void rebuildCBDTTable(std::vector<uint8_t>& cbdtData);
    void rebuildStrike(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    void rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
    
    std::vector<uint8_t> createUpdatedFont(const std::vector<uint8_t>& newCBLCTable, 
                                         const std::vector<uint8_t>& newCBDTTable);
    
    void appendUInt32(std::vector<uint8_t>& data, uint32_t value);
    void appendUInt16(std::vector<uint8_t>& data, uint16_t value);
    void setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value);
};

} // namespace fontmaster

#endif // CBDT_CBLC_REBUILDER_H
