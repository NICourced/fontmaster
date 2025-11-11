#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {

CBDT_CBLC_Rebuilder::CBDT_CBLC_Rebuilder(const std::vector<uint8_t>& fontData, 
                       const std::map<uint32_t, StrikeRecord>& strikes,
                       const std::vector<uint16_t>& removedGlyphs)
    : TTFRebuilder(fontData), strikes(strikes), removedGlyphs(removedGlyphs) {}

std::vector<uint8_t> CBDT_CBLC_Rebuilder::rebuild() {
    std::vector<uint8_t> newCBLCTable;
    std::vector<uint8_t> newCBDTTable;
    
    rebuildCBLCTable(newCBLCTable);
    rebuildCBDTTable(newCBDTTable);
    
    return createUpdatedFont(newCBLCTable, newCBDTTable);
}

void CBDT_CBLC_Rebuilder::rebuildCBLCTable(std::vector<uint8_t>& cblcData) {
    appendUInt32(cblcData, 0x00020000); // version
    appendUInt16(cblcData, static_cast<uint16_t>(strikes.size())); // numStrikes
    
    std::vector<uint32_t> strikeOffsets;
    for (size_t i = 0; i < strikes.size(); ++i) {
        strikeOffsets.push_back(0);
        appendUInt32(cblcData, 0); // placeholder
    }
    
    for (size_t i = 0; i < strikes.size(); ++i) {
        strikeOffsets[i] = cblcData.size();
        rebuildStrike(cblcData, strikes.at(i));
    }
    
    for (size_t i = 0; i < strikes.size(); ++i) {
        uint32_t offsetPos = 8 + i * 4;
        setUInt32(cblcData, offsetPos, strikeOffsets[i]);
    }
}

void CBDT_CBLC_Rebuilder::rebuildStrike(std::vector<uint8_t>& cblcData, const StrikeRecord& strike) {
    size_t strikeStart = cblcData.size();
    
    appendUInt16(cblcData, strike.ppem);
    appendUInt16(cblcData, strike.resolution);
    appendUInt32(cblcData, 0); // colorRef
    appendUInt16(cblcData, 72); // hori (default)
    appendUInt16(cblcData, 72); // vert (default)
    
    uint16_t startGlyph = 0xFFFF;
    uint16_t endGlyph = 0;
    
    for (uint16_t glyphID : strike.glyphIDs) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
            if (glyphID < startGlyph) startGlyph = glyphID;
            if (glyphID > endGlyph) endGlyph = glyphID;
        }
    }
    
    if (startGlyph == 0xFFFF) {
        startGlyph = endGlyph = 0;
    }
    
    appendUInt16(cblcData, startGlyph);
    appendUInt16(cblcData, endGlyph);
    appendUInt16(cblcData, strike.ppem); // ppemX
    appendUInt16(cblcData, strike.ppem); // ppemY
    cblcData.push_back(1); // bitDepth
    appendUInt16(cblcData, 0); // flags
    
    size_t arrayOffsetPos = cblcData.size();
    appendUInt32(cblcData, 0); // placeholder
    
    appendUInt32(cblcData, 1); // numberOfIndexSubTables
    appendUInt32(cblcData, 0); // colorRef
    
    uint32_t arrayOffset = cblcData.size() - strikeStart;
    setUInt32(cblcData, arrayOffsetPos, arrayOffset);
    
    rebuildIndexSubtable1(cblcData, strike);
}

void CBDT_CBLC_Rebuilder::rebuildIndexSubtable1(std::vector<uint8_t>& cblcData, const StrikeRecord& strike) {
    size_t subtableStart = cblcData.size();
    
    uint16_t firstGlyph = 0xFFFF;
    uint16_t lastGlyph = 0;
    
    for (uint16_t glyphID : strike.glyphIDs) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
            if (glyphID < firstGlyph) firstGlyph = glyphID;
            if (glyphID > lastGlyph) lastGlyph = glyphID;
        }
    }
    
    if (firstGlyph == 0xFFFF) {
        firstGlyph = lastGlyph = 0;
    }
    
    appendUInt16(cblcData, firstGlyph);
    appendUInt16(cblcData, lastGlyph);
    
    size_t additionalOffsetPos = cblcData.size();
    appendUInt32(cblcData, 0); // placeholder
    
    uint32_t additionalOffset = cblcData.size() - subtableStart;
    setUInt32(cblcData, additionalOffsetPos, additionalOffset);
    
    appendUInt16(cblcData, 1); // indexFormat
    appendUInt16(cblcData, 17); // imageFormat (PNG with small metrics)
    
    size_t imageDataOffsetPos = cblcData.size();
    appendUInt32(cblcData, 0); // placeholder
    
    uint8_t imageSize = 0;
    int8_t bearingX = 0;
    int8_t bearingY = 0;
    uint8_t advance = 0;
    
    for (uint16_t glyphID : strike.glyphIDs) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
            auto it = strike.glyphImages.find(glyphID);
            if (it != strike.glyphImages.end()) {
                imageSize = std::max(imageSize, static_cast<uint8_t>(it->second.width));
                bearingX = it->second.bearingX;
                bearingY = it->second.bearingY;
                advance = it->second.advance;
                break;
            }
        }
    }
    
    cblcData.push_back(imageSize); // imageSize
    cblcData.push_back(0); // bigMetrics (0 for small)
    cblcData.push_back(static_cast<uint8_t>(bearingX));
    cblcData.push_back(static_cast<uint8_t>(bearingY));
    cblcData.push_back(advance);
    
    uint32_t currentImageOffset = 0;
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) != removedGlyphs.end()) {
            appendUInt32(cblcData, 0);
        } else {
            auto it = strike.glyphImages.find(glyphID);
            if (it != strike.glyphImages.end()) {
                appendUInt32(cblcData, currentImageOffset);
                currentImageOffset += it->second.data.size();
            } else {
                appendUInt32(cblcData, 0);
            }
        }
    }
    
    setUInt32(cblcData, imageDataOffsetPos, cblcData.size() - subtableStart + 4);
}

void CBDT_CBLC_Rebuilder::rebuildCBDTTable(std::vector<uint8_t>& cbdtData) {
    appendUInt32(cbdtData, 0x00020000); // version
    
    for (const auto& strikePair : strikes) {
        for (uint16_t glyphID : strikePair.second.glyphIDs) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
                auto it = strikePair.second.glyphImages.find(glyphID);
                if (it != strikePair.second.glyphImages.end()) {
                    cbdtData.insert(cbdtData.end(), 
                                  it->second.data.begin(), 
                                  it->second.data.end());
                }
            }
        }
    }
}

std::vector<uint8_t> CBDT_CBLC_Rebuilder::createUpdatedFont(const std::vector<uint8_t>& newCBLCTable, 
                                                          const std::vector<uint8_t>& newCBDTTable) {
    std::vector<uint8_t> newFont = originalData;
    
    auto tables = utils::parseTTFTables(newFont);
    
    for (auto& table : tables) {
        std::string tag(table.tag, 4);
        if (tag == "CBLC") {
            if (table.offset + newCBLCTable.size() <= newFont.size()) {
                std::copy(newCBLCTable.begin(), newCBLCTable.end(), 
                         newFont.begin() + table.offset);
            } else {
                std::cerr << "CBLC table size increased, need full rebuild" << std::endl;
            }
        } else if (tag == "CBDT") {
            if (table.offset + newCBDTTable.size() <= newFont.size()) {
                std::copy(newCBDTTable.begin(), newCBDTTable.end(), 
                         newFont.begin() + table.offset);
            } else {
                std::cerr << "CBDT table size increased, need full rebuild" << std::endl;
            }
        }
    }
    
    return newFont;
}

void CBDT_CBLC_Rebuilder::appendUInt32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back((value >> 24) & 0xFF);
    data.push_back((value >> 16) & 0xFF);
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

void CBDT_CBLC_Rebuilder::appendUInt16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

void CBDT_CBLC_Rebuilder::setUInt32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset + 4 <= data.size()) {
        data[offset] = (value >> 24) & 0xFF;
        data[offset + 1] = (value >> 16) & 0xFF;
        data[offset + 2] = (value >> 8) & 0xFF;
        data[offset + 3] = value & 0xFF;
    }
}

} // namespace fontmaster
