#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/NAMEParser.h"
#include <iostream>
#include <algorithm>

namespace fontmaster {

CBDT_CBLC_Parser::CBDT_CBLC_Parser(const std::vector<uint8_t>& fontData) : fontData(fontData) {}

bool CBDT_CBLC_Parser::parse() {
    auto tableRecords = utils::parseTTFTables(fontData);
    
    // Find CBDT and CBLC tables
    auto cbdtTable = std::find_if(tableRecords.begin(), tableRecords.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBDT";
        });
    
    auto cblcTable = std::find_if(tableRecords.begin(), tableRecords.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBLC";
        });
    
    if (cbdtTable == tableRecords.end() || cblcTable == tableRecords.end()) {
        std::cerr << "CBDT/CBLC: Required tables not found" << std::endl;
        return false;
    }
    
    // Parse name table for font information
    auto nameTable = std::find_if(tableRecords.begin(), tableRecords.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "name";
        });
    
    if (nameTable != tableRecords.end()) {
        // Исправлено: убран лишний параметр offset
        utils::NAMEParser nameParser(fontData);
        if (nameParser.parse()) {
            auto postScriptNames = nameParser.getPostScriptNames();
            if (!postScriptNames.empty()) {
                std::cout << "CBDT/CBLC: Font name: " << postScriptNames[0].value << std::endl;
            }
        }
    }
    
    // Parse cmap table for glyph to unicode mapping
    auto cmapTable = std::find_if(tableRecords.begin(), tableRecords.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "cmap";
        });
    
    if (cmapTable != tableRecords.end()) {
        // Исправлено: убран лишний параметр offset
        utils::CMAPParser cmapParser(fontData);
        if (cmapParser.parse()) {
            // Исправлено: правильное имя метода
            glyphIDToUnicode = cmapParser.getGlyphToCharMap();
            std::cout << "CBDT/CBLC: Loaded " << glyphIDToUnicode.size() << " glyph mappings" << std::endl;
        }
    }
    
    // Parse CBLC table first to get strike information
    if (!parseCBLCTable(cblcTable->offset, cblcTable->length)) {
        std::cerr << "CBDT/CBLC: Failed to parse CBLC table" << std::endl;
        return false;
    }
    
    // Parse CBDT table for glyph images
    if (!parseCBDTTable(cbdtTable->offset, cbdtTable->length)) {
        std::cerr << "CBDT/CBLC: Failed to parse CBDT table" << std::endl;
        return false;
    }
    
    std::cout << "CBDT/CBLC: Successfully parsed " << glyphImages.size() << " glyph images" << std::endl;
    return true;
}

bool CBDT_CBLC_Parser::parseCBLCTable(uint32_t offset, uint32_t /*length*/) {
    // Исправлено: убран неиспользуемый параметр length
    
    if (offset + 8 > fontData.size()) {
        std::cerr << "CBLC: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    // Parse CBLC header
    uint32_t version = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    uint32_t numStrikes = (data[4] << 8) | data[5];
    
    std::cout << "CBLC: Version: " << std::hex << version << std::dec 
              << ", Strikes: " << numStrikes << std::endl;
    
    // Parse strike information
    uint32_t strikeDataOffset = offset + 8;
    for (uint32_t i = 0; i < numStrikes; ++i) {
        if (strikeDataOffset + 48 > fontData.size()) {
            std::cerr << "CBLC: Strike data extends beyond font data" << std::endl;
            return false;
        }
        
        StrikeInfo strike;
        const uint8_t* strikeData = fontData.data() + strikeDataOffset;
        
        // Parse strike header (simplified)
        strike.ppem = (strikeData[0] << 8) | strikeData[1];
        strike.resolution = (strikeData[2] << 8) | strikeData[3];
        
        strikes.push_back(strike);
        strikeDataOffset += 48; // Size of strike header
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseCBDTTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        std::cerr << "CBDT: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    // Parse CBDT header
    uint32_t version = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    std::cout << "CBDT: Version: " << std::hex << version << std::dec << std::endl;
    
    // Parse embedded bitmap data for each strike
    uint32_t imageDataOffset = offset + 4;
    for (size_t strikeIndex = 0; strikeIndex < strikes.size(); ++strikeIndex) {
        const auto& strike = strikes[strikeIndex];
        
        // Parse glyph data for this strike (simplified)
        // In real implementation, you'd use the strike information from CBLC
        // to locate and parse individual glyph bitmaps
        
        // For demonstration, just parse a few sample glyphs
        for (uint16_t glyphID = 0; glyphID < 10 && imageDataOffset < offset + length; ++glyphID) {
            if (!parseGlyphImage(static_cast<uint32_t>(strikeIndex), glyphID, imageDataOffset, 0, 0, 0)) {
                break;
            }
            imageDataOffset += 32; // Move to next hypothetical glyph
        }
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseGlyphImage(uint32_t /*strikeIndex*/, uint16_t glyphID, uint32_t imageOffset,
                                      uint16_t /*width*/, uint16_t /*height*/, uint32_t /*format*/) {
    // Исправлено: убран неиспользуемый параметр strikeIndex
    
    if (imageOffset + 4 > fontData.size()) {
        return false;
    }
    
    // Create a simple glyph image record
    GlyphImage image;
    image.glyphID = glyphID;
    image.dataOffset = imageOffset;
    image.dataSize = 32; // Simplified
    
    // Find Unicode value for this glyph
    auto unicodeIt = glyphIDToUnicode.find(glyphID);
    if (unicodeIt != glyphIDToUnicode.end() && !unicodeIt->second.empty()) {
        image.unicodeValue = *unicodeIt->second.begin();
    }
    
    glyphImages.push_back(image);
    
    return true;
}

const std::vector<CBDT_CBLC_Parser::StrikeInfo>& CBDT_CBLC_Parser::getStrikes() const {
    return strikes;
}

const std::vector<CBDT_CBLC_Parser::GlyphImage>& CBDT_CBLC_Parser::getGlyphImages() const {
    return glyphImages;
}

} // namespace fontmaster
