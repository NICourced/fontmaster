#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/NAMEParser.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fontmaster {

// CRC32 table for PNG
static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

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
        if (nameTable->offset + nameTable->length <= fontData.size()) {
            std::vector<uint8_t> nameData(
                fontData.begin() + nameTable->offset,
                fontData.begin() + nameTable->offset + nameTable->length
            );
            utils::NAMEParser nameParser(nameData);
            if (nameParser.parse()) {
                auto postScriptNames = nameParser.getPostScriptNames();
                if (!postScriptNames.empty()) {
                    std::cout << "CBDT/CBLC: Font name: " << postScriptNames[0].value << std::endl;
                }
            }
        }
    }
    
    // Parse cmap table for glyph to unicode mapping
    auto cmapTable = std::find_if(tableRecords.begin(), tableRecords.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "cmap";
        });
    
    if (cmapTable != tableRecords.end()) {
        if (cmapTable->offset + cmapTable->length <= fontData.size()) {
            std::vector<uint8_t> cmapData(
                fontData.begin() + cmapTable->offset,
                fontData.begin() + cmapTable->offset + cmapTable->length
            );
            utils::CMAPParser cmapParser(cmapData);
            if (cmapParser.parse()) {
                glyphIDToUnicode = cmapParser.getGlyphToCharMap();
                std::cout << "CBDT/CBLC: Loaded " << glyphIDToUnicode.size() << " glyph mappings" << std::endl;
            }
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

bool CBDT_CBLC_Parser::parseCBLCTable(uint32_t offset) {
    if (offset + 8 > fontData.size()) {
        std::cerr << "CBLC: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    // Parse CBLC header
    uint32_t version = readUInt32(data);
    uint16_t numStrikes = readUInt16(data + 4);
    
    std::cout << "CBLC: Version: " << std::hex << version << std::dec 
              << ", Strikes: " << numStrikes << std::endl;
    
    if (version != 0x00020000) {
        std::cerr << "CBLC: Unsupported version" << std::endl;
        return false;
    }
    
    // Parse strike offsets
    uint32_t strikeOffsetsOffset = offset + 8;
    std::vector<uint32_t> strikeOffsets;
    
    for (uint16_t i = 0; i < numStrikes; ++i) {
        if (strikeOffsetsOffset + 4 > fontData.size()) {
            std::cerr << "CBLC: Strike offsets extend beyond font data" << std::endl;
            return false;
        }
        
        const uint8_t* offsetData = fontData.data() + strikeOffsetsOffset;
        uint32_t strikeOffset = readUInt32(offsetData);
        strikeOffsets.push_back(strikeOffset);
        strikeOffsetsOffset += 4;
    }
    
    // Parse each strike
    strikeData.resize(numStrikes);
    
    for (uint16_t i = 0; i < numStrikes; ++i) {
        uint32_t strikeOffset = offset + strikeOffsets[i];
        if (!parseStrike(strikeOffset, strikeData[i])) {
            std::cerr << "CBLC: Failed to parse strike " << i << std::endl;
            return false;
        }
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseStrike(uint32_t offset, StrikeData& strike) {
    if (offset + 48 > fontData.size()) {
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    // Parse strike header
    StrikeInfo strikeInfo;
    strikeInfo.ppem = readUInt16(data);
    strikeInfo.resolution = readUInt16(data + 2);
    
    // Parse bitmap size table
    strike.colorRef = readUInt32(data + 4);
    strike.hori = readUInt16(data + 8);
    strike.vert = readUInt16(data + 10);
    strike.startGlyphIndex = readUInt16(data + 12);
    strike.endGlyphIndex = readUInt16(data + 14);
    strike.ppemX = readUInt16(data + 16);
    strike.ppemY = readUInt16(data + 18);
    strike.bitDepth = data[20];
    strike.flags = readUInt16(data + 21);
    
    strikes.push_back(strikeInfo);
    
    // Parse index subtables
    strike.indexSubtableArrayOffset = readUInt32(data + 24);
    strike.numberOfIndexSubTables = readUInt32(data + 28);
    strike.colorRef = readUInt32(data + 32);
    
    uint32_t indexSubtableOffset = offset + strike.indexSubtableArrayOffset;
    
    for (uint32_t i = 0; i < strike.numberOfIndexSubTables; ++i) {
        if (!parseIndexSubtable(indexSubtableOffset, strike)) {
            std::cerr << "CBLC: Failed to parse index subtable " << i << std::endl;
            return false;
        }
        indexSubtableOffset += 8;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseIndexSubtable(uint32_t offset, StrikeData& strike) {
    if (offset + 8 > fontData.size()) {
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint16_t firstGlyphIndex = readUInt16(data);
    uint16_t lastGlyphIndex = readUInt16(data + 2);
    uint32_t additionalOffsetToIndexSubtable = readUInt32(data + 4);
    
    uint32_t subtableOffset = offset + additionalOffsetToIndexSubtable;
    if (subtableOffset + 8 > fontData.size()) {
        return false;
    }
    
    const uint8_t* subtableData = fontData.data() + subtableOffset;
    uint16_t indexFormat = readUInt16(subtableData);
    uint16_t imageFormat = readUInt16(subtableData + 2);
    uint32_t imageDataOffset = readUInt32(subtableData + 4);
    
    switch (indexFormat) {
        case 1:
            return parseIndexFormat1(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 2:
            return parseIndexFormat2(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 3:
            return parseIndexFormat3(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 4:
            return parseIndexFormat4(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        case 5:
            return parseIndexFormat5(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
        default:
            std::cerr << "CBLC: Unsupported index format: " << indexFormat << std::endl;
            return false;
    }
}

bool CBDT_CBLC_Parser::parseIndexFormat1(uint32_t offset, StrikeData& strike, 
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    const uint8_t* data = fontData.data() + offset + 8;
    
    uint8_t imageSize = data[0];
    int8_t bigMetrics = data[1];
    int8_t bearingX = static_cast<int8_t>(data[2]);
    int8_t bearingY = static_cast<int8_t>(data[3]);
    uint8_t advance = data[4];
    
    uint32_t glyphDataOffset = offset + 8 + 5;
    
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (glyphDataOffset + 4 > fontData.size()) {
            return false;
        }
        
        const uint8_t* glyphOffsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(glyphOffsetData);
        
        GlyphRecord record;
        record.glyphID = glyphID;
        record.imageFormat = static_cast<uint8_t>(imageFormat);
        record.imageOffset = imageDataOffset + glyphImageOffset;
        
        if (bigMetrics) {
            record.width = imageSize;
            record.height = imageSize;
        } else {
            record.width = (imageSize + 7) / 8;
            record.height = imageSize;
        }
        
        record.bearingX = bearingX;
        record.bearingY = bearingY;
        record.advance = advance;
        record.imageLength = calculateImageSize(record.width, record.height, imageFormat);
        
        strike.glyphRecords.push_back(record);
        glyphDataOffset += 4;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseIndexFormat2(uint32_t offset, StrikeData& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    
    uint32_t glyphDataOffset = offset + 8;
    
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (glyphDataOffset + 6 > fontData.size()) {
            return false;
        }
        
        const uint8_t* glyphData = fontData.data() + glyphDataOffset;
        
        GlyphRecord record;
        record.glyphID = glyphID;
        record.imageFormat = static_cast<uint8_t>(imageFormat);
        
        uint32_t glyphImageOffset = readUInt32(glyphData);
        record.imageOffset = imageDataOffset + glyphImageOffset;
        
        uint8_t imageSize = glyphData[4];
        int8_t bigMetrics = glyphData[5];
        
        if (glyphDataOffset + 6 + (bigMetrics ? 4 : 2) > fontData.size()) {
            return false;
        }
        
        if (bigMetrics) {
            record.width = readUInt16(glyphData + 6);
            record.height = readUInt16(glyphData + 8);
            record.bearingX = static_cast<int16_t>(readUInt16(glyphData + 10));
            record.bearingY = static_cast<int16_t>(readUInt16(glyphData + 12));
            record.advance = readUInt16(glyphData + 14);
            glyphDataOffset += 16;
        } else {
            record.width = imageSize;
            record.height = imageSize;
            record.bearingX = static_cast<int8_t>(glyphData[6]);
            record.bearingY = static_cast<int8_t>(glyphData[7]);
            record.advance = glyphData[8];
            glyphDataOffset += 9;
        }
        
        record.imageLength = calculateImageSize(record.width, record.height, imageFormat);
        strike.glyphRecords.push_back(record);
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseIndexFormat3(uint32_t offset, StrikeData& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    return parseIndexFormat1(offset, strike, firstGlyph, lastGlyph, imageFormat, imageDataOffset);
}

bool CBDT_CBLC_Parser::parseIndexFormat4(uint32_t offset, StrikeData& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    return parseIndexFormat2(offset, strike, firstGlyph, lastGlyph, imageFormat, imageDataOffset);
}

bool CBDT_CBLC_Parser::parseIndexFormat5(uint32_t offset, StrikeData& strike,
                                       uint16_t firstGlyph, uint16_t lastGlyph,
                                       uint16_t imageFormat, uint32_t imageDataOffset) {
    
    uint32_t glyphDataOffset = offset + 8;
    uint16_t numGlyphs = lastGlyph - firstGlyph + 1;
    
    std::vector<uint16_t> glyphIDs;
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 2 > fontData.size()) {
            return false;
        }
        
        const uint8_t* glyphIDData = fontData.data() + glyphDataOffset;
        uint16_t glyphID = readUInt16(glyphIDData);
        glyphIDs.push_back(glyphID);
        glyphDataOffset += 2;
    }
    
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 4 > fontData.size()) {
            return false;
        }
        
        const uint8_t* offsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(offsetData);
        
        GlyphRecord record;
        record.glyphID = glyphIDs[i];
        record.imageFormat = static_cast<uint8_t>(imageFormat);
        record.imageOffset = imageDataOffset + glyphImageOffset;
        record.width = 0;
        record.height = 0;
        record.bearingX = 0;
        record.bearingY = 0;
        record.advance = 0;
        record.imageLength = 0;
        
        strike.glyphRecords.push_back(record);
        glyphDataOffset += 4;
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseCBDTTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        std::cerr << "CBDT: Table too small" << std::endl;
        return false;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint32_t version = readUInt32(data);
    
    std::cout << "CBDT: Version: " << std::hex << version << std::dec << std::endl;
    
    if (version != 0x00020000 && version != 0x00030000) {
        std::cerr << "CBDT: Unsupported version" << std::endl;
        return false;
    }
    
    for (size_t strikeIndex = 0; strikeIndex < strikeData.size(); ++strikeIndex) {
        const auto& strike = strikeData[strikeIndex];
        
        std::cout << "CBDT: Processing strike " << strikeIndex 
                  << " (glyphs: " << strike.glyphRecords.size() << ")" << std::endl;
        
        for (const auto& glyphRecord : strike.glyphRecords) {
            if (glyphRecord.imageOffset >= offset + length) {
                std::cerr << "CBDT: Glyph image offset beyond table bounds" << std::endl;
                continue;
            }
            
            if (!parseGlyphImage(static_cast<uint32_t>(strikeIndex), 
                               glyphRecord.glyphID, 
                               glyphRecord.imageOffset,
                               glyphRecord.width,
                               glyphRecord.height,
                               glyphRecord.imageFormat)) {
                std::cerr << "CBDT: Failed to parse glyph " << glyphRecord.glyphID << std::endl;
                continue;
            }
        }
    }
    
    return true;
}

bool CBDT_CBLC_Parser::parseGlyphImage(uint32_t strikeIndex, uint16_t glyphID, uint32_t imageOffset,
                                      uint16_t width, uint16_t height, uint32_t format) {
    
    if (imageOffset >= fontData.size()) {
        return false;
    }
    
    const uint8_t* imageData = fontData.data() + imageOffset;
    
    GlyphImage image;
    image.glyphID = glyphID;
    image.strikeIndex = strikeIndex;
    image.dataOffset = imageOffset;
    image.width = width;
    image.height = height;
    image.format = static_cast<uint8_t>(format);
    
    auto unicodeIt = glyphIDToUnicode.find(glyphID);
    if (unicodeIt != glyphIDToUnicode.end() && !unicodeIt->second.empty()) {
        image.unicodeValue = *unicodeIt->second.begin();
    }
    
    bool success = false;
    switch (format) {
        case 1: success = parseFormat1(imageData, image); break;
        case 2: success = parseFormat2(imageData, image); break;
        case 3: success = parseFormat3(imageData, image); break;
        case 4: success = parseFormat4(imageData, image); break;
        case 5: success = parseFormat5(imageData, image); break;
        case 6: success = parseFormat6(imageData, image); break;
        case 7: success = parseFormat7(imageData, image); break;
        case 8: success = parseFormat8(imageData, image); break;
        case 9: success = parseFormat9(imageData, image); break;
        case 17: success = parseFormat17(imageData, image); break;
        case 18: success = parseFormat18(imageData, image); break;
        default:
            std::cerr << "CBDT: Unsupported image format: " << format << std::endl;
            return false;
    }
    
    if (success) {
        glyphImages.push_back(image);
        std::cout << "CBDT: Parsed glyph " << glyphID 
                  << " (strike " << strikeIndex 
                  << ", " << image.width << "x" << image.height 
                  << ", format: " << format << ")" << std::endl;
    }
    
    return success;
}

// Basic bitmap formats
bool CBDT_CBLC_Parser::parseFormat1(const uint8_t* data, GlyphImage& image) {
    if (data + 5 > fontData.data() + fontData.size()) {
        return false;
    }
    
    uint8_t smallMetrics = data[0];
    image.bearingX = static_cast<int8_t>(data[1]);
    image.bearingY = static_cast<int8_t>(data[2]);
    image.advance = data[3];
    
    image.width = (smallMetrics & 0xF0) >> 4;
    image.height = smallMetrics & 0x0F;
    
    uint32_t dataSize = ((image.width + 7) / 8) * image.height;
    if (data + 4 + dataSize > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.dataSize = 4 + dataSize;
    image.bitmapData.assign(data + 4, data + 4 + dataSize);
    
    return true;
}

bool CBDT_CBLC_Parser::parseFormat2(const uint8_t* data, GlyphImage& image) {
    if (data + 10 > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.width = readUInt16(data);
    image.height = readUInt16(data + 2);
    image.bearingX = static_cast<int16_t>(readUInt16(data + 4));
    image.bearingY = static_cast<int16_t>(readUInt16(data + 6));
    image.advance = readUInt16(data + 8);
    
    uint32_t dataSize = ((image.width + 7) / 8) * image.height;
    if (data + 10 + dataSize > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.dataSize = 10 + dataSize;
    image.bitmapData.assign(data + 10, data + 10 + dataSize);
    
    return true;
}

bool CBDT_CBLC_Parser::parseFormat3(const uint8_t* data, GlyphImage& image) {
    return parseFormat1(data, image);
}

bool CBDT_CBLC_Parser::parseFormat4(const uint8_t* data, GlyphImage& image) {
    return parseFormat2(data, image);
}

// PNG format
bool CBDT_CBLC_Parser::parseFormat5(const uint8_t* data, GlyphImage& image) {
    uint32_t maxLength = static_cast<uint32_t>(fontData.size() - (data - fontData.data()));
    return parsePNGData(data, maxLength, image);
}

// JPEG format
bool CBDT_CBLC_Parser::parseFormat6(const uint8_t* data, GlyphImage& image) {
    if (data + 2 > fontData.data() + fontData.size()) {
        return false;
    }
    
    if (data[0] != 0xFF || data[1] != 0xD8) {
        std::cerr << "CBDT: Invalid JPEG signature" << std::endl;
        return false;
    }
    
    uint32_t jpegLength = 0;
    const uint8_t* current = data;
    while (current + 2 <= fontData.data() + fontData.size()) {
        if (current[0] == 0xFF && current[1] == 0xD9) {
            jpegLength = (current - data) + 2;
            break;
        }
        current++;
    }
    
    if (jpegLength == 0) {
        std::cerr << "CBDT: Could not find JPEG end marker" << std::endl;
        return false;
    }
    
    image.dataSize = jpegLength;
    image.bitmapData.assign(data, data + jpegLength);
    
    return true;
}

// TIFF format
bool CBDT_CBLC_Parser::parseFormat7(const uint8_t* data, GlyphImage& image) {
    uint32_t maxLength = static_cast<uint32_t>(fontData.size() - (data - fontData.data()));
    return parseTIFFData(data, maxLength, image);
}

bool CBDT_CBLC_Parser::parseFormat8(const uint8_t* data, GlyphImage& image) {
    uint32_t dataSize = calculateImageSize(image.width, image.height, 8);
    if (data + dataSize > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.dataSize = dataSize;
    image.bitmapData.assign(data, data + dataSize);
    
    return true;
}

bool CBDT_CBLC_Parser::parseFormat9(const uint8_t* data, GlyphImage& image) {
    uint32_t dataSize = calculateImageSize(image.width, image.height, 9);
    if (data + dataSize > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.dataSize = dataSize;
    image.bitmapData.assign(data, data + dataSize);
    
    return true;
}

bool CBDT_CBLC_Parser::parseFormat17(const uint8_t* data, GlyphImage& image) {
    if (!parseFormat1(data, image)) {
        return false;
    }
    return parseFormat5(data + 4, image);
}

bool CBDT_CBLC_Parser::parseFormat18(const uint8_t* data, GlyphImage& image) {
    if (!parseFormat2(data, image)) {
        return false;
    }
    return parseFormat5(data + 10, image);
}

// PNG parsing implementation
bool CBDT_CBLC_Parser::parsePNGData(const uint8_t* data, uint32_t length, GlyphImage& image) {
    if (length < 8) {
        return false;
    }
    
    // Check PNG signature
    if (data[0] != 0x89 || data[1] != 0x50 || data[2] != 0x4E || data[3] != 0x47 ||
        data[4] != 0x0D || data[5] != 0x0A || data[6] != 0x1A || data[7] != 0x0A) {
        std::cerr << "CBDT: Invalid PNG signature" << std::endl;
        return false;
    }
    
    image.bitmapData.assign(data, data + 8); // Include signature
    
    const uint8_t* current = data + 8;
    uint32_t remaining = length - 8;
    
    while (remaining >= 12) { // Minimum chunk size: 4 (length) + 4 (type) + 4 (CRC)
        uint32_t chunkLength = readUInt32(current);
        const uint8_t* chunkType = current + 4;
        
        if (chunkLength > remaining - 12) {
            break; // Chunk extends beyond available data
        }
        
        // Parse important chunks
        if (std::memcmp(chunkType, "IHDR", 4) == 0) {
            if (!parseIHDRChunk(current + 8, image)) {
                return false;
            }
        }
        
        // Copy chunk to output
        image.bitmapData.insert(image.bitmapData.end(), current, current + 12 + chunkLength);
        
        // Check for IEND chunk
        if (std::memcmp(chunkType, "IEND", 4) == 0) {
            image.dataSize = static_cast<uint32_t>(image.bitmapData.size());
            return true;
        }
        
        uint32_t chunkTotalSize = 12 + chunkLength;
        current += chunkTotalSize;
        remaining -= chunkTotalSize;
    }
    
    std::cerr << "CBDT: Incomplete PNG data" << std::endl;
    return false;
}

bool CBDT_CBLC_Parser::parseIHDRChunk(const uint8_t* data, GlyphImage& image) {
    if (data + 13 > fontData.data() + fontData.size()) {
        return false;
    }
    
    image.pngWidth = readUInt32(data);
    image.pngHeight = readUInt32(data + 4);
    image.pngBitDepth = data[8];
    image.pngColorType = data[9];
    
    // Update image dimensions from PNG
    image.width = static_cast<uint16_t>(image.pngWidth);
    image.height = static_cast<uint16_t>(image.pngHeight);
    
    return true;
}

uint32_t CBDT_CBLC_Parser::crc32(const uint8_t* data, uint32_t length) {
    uint32_t crc = 0xffffffff;
    for (uint32_t i = 0; i < length; ++i) {
        crc = crc_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffff;
}

// TIFF parsing implementation
bool CBDT_CBLC_Parser::parseTIFFData(const uint8_t* data, uint32_t length, GlyphImage& image) {
    if (length < 8) {
        return false;
    }
    
    return parseTIFFHeader(data, image);
}

bool CBDT_CBLC_Parser::parseTIFFHeader(const uint8_t* data, GlyphImage& image) {
    // Check TIFF signature
    bool bigEndian = false;
    if (data[0] == 0x4D && data[1] == 0x4D) { // "MM" - big endian
        bigEndian = true;
    } else if (data[0] == 0x49 && data[1] == 0x49) { // "II" - little endian
        bigEndian = false;
    } else {
        std::cerr << "CBDT: Invalid TIFF signature" << std::endl;
        return false;
    }
    
    // Check TIFF magic number
    uint16_t magic = readTIFFShort(data + 2, bigEndian);
    if (magic != 42) {
        std::cerr << "CBDT: Invalid TIFF magic number" << std::endl;
        return false;
    }
    
    // Get offset to first IFD
    uint32_t ifdOffset = readTIFFLong(data + 4, bigEndian);
    
    if (ifdOffset >= fontData.size() - (data - fontData.data())) {
        std::cerr << "CBDT: TIFF IFD offset out of bounds" << std::endl;
        return false;
    }
    
    // Store TIFF data
    uint32_t dataSize = std::min(static_cast<uint32_t>(1024 * 1024), 
                                static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    image.dataSize = dataSize;
    image.bitmapData.assign(data, data + dataSize);
    
    // Parse first IFD
    return parseTIFFDirectory(data, ifdOffset, image);
}

bool CBDT_CBLC_Parser::parseTIFFDirectory(const uint8_t* data, uint32_t offset, GlyphImage& image) {
    const uint8_t* ifd = data + offset;
    bool bigEndian = (data[0] == 0x4D && data[1] == 0x4D);
    
    uint16_t entryCount = readTIFFShort(ifd, bigEndian);
    
    if (ifd + 2 + entryCount * 12 > data + image.dataSize) {
        return false;
    }
    
    const uint8_t* entries = ifd + 2;
    
    for (uint16_t i = 0; i < entryCount; ++i) {
        const uint8_t* entry = entries + i * 12;
        uint16_t tag = readTIFFShort(entry, bigEndian);
        uint16_t type = readTIFFShort(entry + 2, bigEndian);
        uint32_t count = readTIFFLong(entry + 4, bigEndian);
        
        // Read important tags
        switch (tag) {
            case 256: // ImageWidth
                if (type == 3 || type == 4) {
                    image.tiffWidth = readTIFFLong(entry + 8, bigEndian);
                    image.width = static_cast<uint16_t>(image.tiffWidth);
                }
                break;
            case 257: // ImageLength
                if (type == 3 || type == 4) {
                    image.tiffHeight = readTIFFLong(entry + 8, bigEndian);
                    image.height = static_cast<uint16_t>(image.tiffHeight);
                }
                break;
            case 259: // Compression
                if (type == 3) {
                    image.tiffCompression = readTIFFShort(entry + 8, bigEndian);
                }
                break;
            case 262: // PhotometricInterpretation
                if (type == 3) {
                    image.tiffPhotometric = readTIFFShort(entry + 8, bigEndian);
                }
                break;
        }
    }
    
    return true;
}

uint16_t CBDT_CBLC_Parser::readTIFFShort(const uint8_t* data, bool bigEndian) {
    if (bigEndian) {
        return (data[0] << 8) | data[1];
    } else {
        return (data[1] << 8) | data[0];
    }
}

uint32_t CBDT_CBLC_Parser::readTIFFLong(const uint8_t* data, bool bigEndian) {
    if (bigEndian) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    } else {
        return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
    }
}

// Utility functions
uint32_t CBDT_CBLC_Parser::calculateImageSize(uint16_t width, uint16_t height, uint32_t format) {
    switch (format) {
        case 1: case 3: case 8:
            return ((width + 7) / 8) * height;
        case 2: case 4: case 9:
            return ((width * height) + 7) / 8;
        case 5: case 6: case 7: case 17: case 18:
            return 0;
        default:
            return 0;
    }
}

uint32_t CBDT_CBLC_Parser::readUInt24(const uint8_t* data) {
    return (data[0] << 16) | (data[1] << 8) | data[2];
}

uint32_t CBDT_CBLC_Parser::readUInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint16_t CBDT_CBLC_Parser::readUInt16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

const std::vector<CBDT_CBLC_Parser::StrikeInfo>& CBDT_CBLC_Parser::getStrikes() const {
    return strikes;
}

const std::vector<CBDT_CBLC_Parser::GlyphImage>& CBDT_CBLC_Parser::getGlyphImages() const {
    return glyphImages;
}

const std::vector<CBDT_CBLC_Parser::StrikeData>& CBDT_CBLC_Parser::getStrikeData() const {
    return strikeData;
}

} // namespace fontmaster
