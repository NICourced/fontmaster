#pragma once
#include "fontmaster/TTFRebuilder.h"
#include <vector>
#include <map>
#include <string>

namespace fontmaster {

class CBDT_CBLC_Rebuilder : public TTFRebuilder {
private:
    std::map<std::string, std::vector<CBDTStrike>> glyphStrikes;
    std::vector<std::string> removedGlyphs;
    std::map<uint32_t, StrikeRecord> strikes;
    
public:
    CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData, 
                       const std::map<std::string, std::vector<CBDTStrike>>& strikes,
                       const std::vector<std::string>& removed);
    
    std::vector<uint8_t> rebuildFont();
    
private:
    void rebuildCBDTTable();
    void rebuildCBLCTable();
    void rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike);
};

}
