#include "fontmaster/CBDT_CBLC_Handler.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/FontMasterImpl.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/NAMEParser.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>

namespace fontmaster {

CBDT_CBLC_Handler::CBDT_CBLC_Handler() {
    supportedFormats = {"CBDT", "CBLC"};
}

bool CBDT_CBLC_Handler::canHandle(const std::vector<uint8_t>& data) const {
    auto tables = utils::parseTTFTables(data);
    
    bool hasCBDT = false;
    bool hasCBLC = false;
    
    for (const auto& table : tables) {
        std::string tag(table.tag, 4);
        if (tag == "CBDT") hasCBDT = true;
        if (tag == "CBLC") hasCBLC = true;
    }
    
    return hasCBDT && hasCBLC;
}

std::unique_ptr<Font> CBDT_CBLC_Handler::loadFont(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "CBDT/CBLC: Cannot open file: " << filepath << std::endl;
        return nullptr;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    if (!file) {
        std::cerr << "CBDT/CBLC: Cannot read file: " << filepath << std::endl;
        return nullptr;
    }
    
    if (!canHandle(data)) {
        std::cerr << "CBDT/CBLC: Cannot handle this font format" << std::endl;
        return nullptr;
    }
    
    auto font = std::make_unique<CBDT_CBLC_Font>(filepath);
    font->setFontData(data);
    
    if (!font->load()) {
        std::cerr << "CBDT/CBLC: Failed to load font" << std::endl;
        return nullptr;
    }
    
    return font;
}

// Реализация CBDT_CBLC_Font
CBDT_CBLC_Font::CBDT_CBLC_Font(const std::string& filepath) 
    : filepath(filepath) {}

bool CBDT_CBLC_Font::load() {
    if (fontData.empty()) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return false;
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        fontData.resize(size);
        file.read(reinterpret_cast<char*>(fontData.data()), size);
    }
    
    auto tables = utils::parseTTFTables(fontData);
    
    // Парсинг CBLC таблицы
    auto cblcTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBLC";
        });
    
    if (cblcTable != tables.end()) {
        parseCBLCTable(cblcTable->offset, cblcTable->length);
    }
    
    // Парсинг CBDT таблицы
    auto cbdtTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "CBDT";
        });
    
    if (cbdtTable != tables.end()) {
        parseCBDTTable(cbdtTable->offset, cbdtTable->length);
    }
    
    // Парсинг cmap для определения удаленных глифов
    auto cmapTable = std::find_if(tables.begin(), tables.end(),
        [](const utils::TableRecord& record) {
            return std::string(record.tag, 4) == "cmap";
        });
    
    if (cmapTable != tables.end()) {
        parseCMAPTable(cmapTable->offset, cmapTable->length);
    }
    
    std::cout << "CBDT/CBLC Font loaded: " << filepath 
              << ", Strikes: " << glyphStrikes.size() 
              << ", Removed glyphs: " << removedGlyphs.size() << std::endl;
    
    return true;
}

void CBDT_CBLC_Font::parseCBLCTable(uint32_t offset, uint32_t length) {
    if (offset + 8 > fontData.size()) {
        std::cerr << "CBLC: Table too small" << std::endl;
        return;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint32_t version = readUInt32(data);
    uint16_t numStrikes = readUInt16(data + 4);
    
    std::cout << "CBLC: Version: " << std::hex << version << std::dec 
              << ", Strikes: " << numStrikes << std::endl;
    
    if (version != 0x00020000) {
        std::cerr << "CBLC: Unsupported version" << std::endl;
        return;
    }
    
    // Парсинг информации о страйках
    uint32_t strikeOffsetsOffset = offset + 8;
    
    for (uint16_t i = 0; i < numStrikes; ++i) {
        if (strikeOffsetsOffset + 4 > fontData.size()) {
            break;
        }
        
        const uint8_t* offsetData = fontData.data() + strikeOffsetsOffset;
        uint32_t strikeOffset = readUInt32(offsetData);
        
        parseStrike(offset + strikeOffset, i);
        strikeOffsetsOffset += 4;
    }
}

void CBDT_CBLC_Font::parseStrike(uint32_t offset, uint32_t strikeIndex) {
    if (offset + 48 > fontData.size()) {
        return;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    StrikeRecord strike;
    strike.ppem = readUInt16(data);
    strike.resolution = readUInt16(data + 2);
    
    // Пропускаем colorRef
    uint16_t startGlyphIndex = readUInt16(data + 12);
    uint16_t endGlyphIndex = readUInt16(data + 14);
    uint16_t ppemX = readUInt16(data + 16);
    uint16_t ppemY = readUInt16(data + 18);
    uint8_t bitDepth = data[20];
    uint16_t flags = readUInt16(data + 21);
    
    uint32_t indexSubtableArrayOffset = readUInt32(data + 24);
    uint32_t numberOfIndexSubTables = readUInt32(data + 28);
    
    uint32_t indexSubtableOffset = offset + indexSubtableArrayOffset;
    
    for (uint32_t i = 0; i < numberOfIndexSubTables; ++i) {
        if (indexSubtableOffset + 8 > fontData.size()) {
            break;
        }
        
        parseIndexSubtable(indexSubtableOffset, strike);
        indexSubtableOffset += 8;
    }
    
    glyphStrikes[strikeIndex] = strike;
}

void CBDT_CBLC_Font::parseIndexSubtable(uint32_t offset, StrikeRecord& strike) {
    if (offset + 8 > fontData.size()) {
        return;
    }
    
    const uint8_t* data = fontData.data() + offset;
    
    uint16_t firstGlyphIndex = readUInt16(data);
    uint16_t lastGlyphIndex = readUInt16(data + 2);
    uint32_t additionalOffsetToIndexSubtable = readUInt32(data + 4);
    
    uint32_t subtableOffset = offset + additionalOffsetToIndexSubtable;
    if (subtableOffset + 8 > fontData.size()) {
        return;
    }
    
    const uint8_t* subtableData = fontData.data() + subtableOffset;
    uint16_t indexFormat = readUInt16(subtableData);
    uint16_t imageFormat = readUInt16(subtableData + 2);
    uint32_t imageDataOffset = readUInt32(subtableData + 4);
    
    switch (indexFormat) {
        case 1:
            parseIndexFormat1(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
            break;
        case 2:
            parseIndexFormat2(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
            break;
        case 5:
            parseIndexFormat5(subtableOffset, strike, firstGlyphIndex, lastGlyphIndex, imageFormat, imageDataOffset);
            break;
        default:
            std::cout << "Unsupported index format: " << indexFormat << std::endl;
            break;
    }
}

void CBDT_CBLC_Font::parseIndexFormat1(uint32_t offset, StrikeRecord& strike, 
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
            break;
        }
        
        const uint8_t* glyphOffsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(glyphOffsetData);
        
        strike.glyphIDs.push_back(glyphID);
        
        GlyphData glyphData;
        glyphData.imageFormat = imageFormat;
        glyphData.imageOffset = imageDataOffset + glyphImageOffset;
        glyphData.bearingX = bearingX;
        glyphData.bearingY = bearingY;
        glyphData.advance = advance;
        
        if (bigMetrics) {
            glyphData.width = imageSize;
            glyphData.height = imageSize;
        } else {
            glyphData.width = (imageSize + 7) / 8;
            glyphData.height = imageSize;
        }
        
        strike.glyphData[glyphID] = glyphData;
        glyphDataOffset += 4;
    }
}

void CBDT_CBLC_Font::parseIndexFormat2(uint32_t offset, StrikeRecord& strike,
                                     uint16_t firstGlyph, uint16_t lastGlyph,
                                     uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (glyphDataOffset + 6 > fontData.size()) {
            break;
        }
        
        const uint8_t* glyphDataPtr = fontData.data() + glyphDataOffset;
        
        uint32_t glyphImageOffset = readUInt32(glyphDataPtr);
        uint8_t imageSize = glyphDataPtr[4];
        int8_t bigMetrics = glyphDataPtr[5];
        
        GlyphData glyphData;
        glyphData.imageFormat = imageFormat;
        glyphData.imageOffset = imageDataOffset + glyphImageOffset;
        
        if (bigMetrics) {
            if (glyphDataOffset + 16 > fontData.size()) {
                break;
            }
            glyphData.width = readUInt16(glyphDataPtr + 6);
            glyphData.height = readUInt16(glyphDataPtr + 8);
            glyphData.bearingX = static_cast<int16_t>(readUInt16(glyphDataPtr + 10));
            glyphData.bearingY = static_cast<int16_t>(readUInt16(glyphDataPtr + 12));
            glyphData.advance = readUInt16(glyphDataPtr + 14);
            glyphDataOffset += 16;
        } else {
            if (glyphDataOffset + 9 > fontData.size()) {
                break;
            }
            glyphData.width = imageSize;
            glyphData.height = imageSize;
            glyphData.bearingX = static_cast<int8_t>(glyphDataPtr[6]);
            glyphData.bearingY = static_cast<int8_t>(glyphDataPtr[7]);
            glyphData.advance = glyphDataPtr[8];
            glyphDataOffset += 9;
        }
        
        strike.glyphIDs.push_back(glyphID);
        strike.glyphData[glyphID] = glyphData;
    }
}

void CBDT_CBLC_Font::parseIndexFormat5(uint32_t offset, StrikeRecord& strike,
                                     uint16_t firstGlyph, uint16_t lastGlyph,
                                     uint16_t imageFormat, uint32_t imageDataOffset) {
    uint32_t glyphDataOffset = offset + 8;
    uint16_t numGlyphs = lastGlyph - firstGlyph + 1;
    
    std::vector<uint16_t> glyphIDs;
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 2 > fontData.size()) {
            break;
        }
        
        const uint8_t* glyphIDData = fontData.data() + glyphDataOffset;
        uint16_t glyphID = readUInt16(glyphIDData);
        glyphIDs.push_back(glyphID);
        glyphDataOffset += 2;
    }
    
    for (uint16_t i = 0; i < numGlyphs; ++i) {
        if (glyphDataOffset + 4 > fontData.size()) {
            break;
        }
        
        const uint8_t* offsetData = fontData.data() + glyphDataOffset;
        uint32_t glyphImageOffset = readUInt32(offsetData);
        
        GlyphData glyphData;
        glyphData.imageFormat = imageFormat;
        glyphData.imageOffset = imageDataOffset + glyphImageOffset;
        glyphData.width = 0;
        glyphData.height = 0;
        glyphData.bearingX = 0;
        glyphData.bearingY = 0;
        glyphData.advance = 0;
        
        strike.glyphIDs.push_back(glyphIDs[i]);
        strike.glyphData[glyphIDs[i]] = glyphData;
        glyphDataOffset += 4;
    }
}

void CBDT_CBLC_Font::parseCBDTTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        std::cerr << "CBDT: Table too small" << std::endl;
        return;
    }
    
    const uint8_t* data = fontData.data() + offset;
    uint32_t version = readUInt32(data);
    
    std::cout << "CBDT: Version: " << std::hex << version << std::dec << std::endl;
    
    for (auto& strikePair : glyphStrikes) {
        auto& strike = strikePair.second;
        
        for (auto& glyphPair : strike.glyphData) {
            auto& glyphData = glyphPair.second;
            
            if (glyphData.imageOffset >= offset + length) {
                continue;
            }
            
            extractGlyphImageData(offset + glyphData.imageOffset, glyphData);
        }
    }
}

void CBDT_CBLC_Font::extractGlyphImageData(uint32_t imageOffset, GlyphData& glyphData) {
    if (imageOffset >= fontData.size()) {
        return;
    }
    
    const uint8_t* imageData = fontData.data() + imageOffset;
    
    switch (glyphData.imageFormat) {
        case 1: case 3: case 8:
            extractBitmapData(imageData, glyphData);
            break;
        case 2: case 4: case 9:
            extractBitmapData(imageData, glyphData);
            break;
        case 5: case 17: case 18:
            extractPNGData(imageData, glyphData);
            break;
        case 6:
            extractJPEGData(imageData, glyphData);
            break;
        case 7:
            extractTIFFData(imageData, glyphData);
            break;
        default:
            std::cout << "Unsupported image format: " << glyphData.imageFormat << std::endl;
            break;
    }
}

void CBDT_CBLC_Font::extractBitmapData(const uint8_t* data, GlyphData& glyphData) {
    uint32_t estimatedSize = calculateBitmapSize(glyphData.width, glyphData.height, glyphData.imageFormat);
    uint32_t actualSize = std::min(estimatedSize, static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    
    glyphData.imageData.assign(data, data + actualSize);
}

void CBDT_CBLC_Font::extractPNGData(const uint8_t* data, GlyphData& glyphData) {
    const uint8_t* current = data;
    uint32_t pngLength = 0;
    
    while (current + 8 <= fontData.data() + fontData.size()) {
        if (current[0] == 0x49 && current[1] == 0x45 && current[2] == 0x4E && current[3] == 0x44) {
            pngLength = (current - data) + 12;
            break;
        }
        current++;
    }
    
    if (pngLength == 0) {
        pngLength = std::min(static_cast<uint32_t>(1024 * 1024), 
                           static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    }
    
    glyphData.imageData.assign(data, data + pngLength);
}

void CBDT_CBLC_Font::extractJPEGData(const uint8_t* data, GlyphData& glyphData) {
    const uint8_t* current = data;
    uint32_t jpegLength = 0;
    
    while (current + 2 <= fontData.data() + fontData.size()) {
        if (current[0] == 0xFF && current[1] == 0xD9) {
            jpegLength = (current - data) + 2;
            break;
        }
        current++;
    }
    
    if (jpegLength == 0) {
        jpegLength = std::min(static_cast<uint32_t>(1024 * 1024), 
                            static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    }
    
    glyphData.imageData.assign(data, data + jpegLength);
}

void CBDT_CBLC_Font::extractTIFFData(const uint8_t* data, GlyphData& glyphData) {
    uint32_t tiffLength = std::min(static_cast<uint32_t>(1024 * 1024), 
                                 static_cast<uint32_t>(fontData.size() - (data - fontData.data())));
    
    glyphData.imageData.assign(data, data + tiffLength);
}

void CBDT_CBLC_Font::parseCMAPTable(uint32_t offset, uint32_t length) {
    if (offset + 4 > fontData.size()) {
        return;
    }
    
    std::vector<uint8_t> cmapData(fontData.begin() + offset, 
                                 fontData.begin() + offset + length);
    
    utils::CMAPParser cmapParser(cmapData);
    if (cmapParser.parse()) {
        auto glyphToCharMap = cmapParser.getGlyphToCharMap();
        
        for (const auto& strikePair : glyphStrikes) {
            for (uint16_t glyphID : strikePair.second.glyphIDs) {
                if (glyphToCharMap.find(glyphID) == glyphToCharMap.end() || 
                    glyphToCharMap[glyphID].empty()) {
                    if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
                        removedGlyphs.push_back(glyphID);
                    }
                }
            }
        }
    }
}

uint32_t CBDT_CBLC_Font::calculateBitmapSize(uint16_t width, uint16_t height, uint16_t format) {
    switch (format) {
        case 1: case 3: case 8:
            return ((width + 7) / 8) * height;
        case 2: case 4: case 9:
            return ((width * height) + 7) / 8;
        default:
            return 1024;
    }
}

uint32_t CBDT_CBLC_Font::readUInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint16_t CBDT_CBLC_Font::readUInt16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

bool CBDT_CBLC_Font::save(const std::string& filepath) {
    CBDT_CBLC_Rebuilder rebuilder(fontData, glyphStrikes, removedGlyphs);
    std::vector<uint8_t> newData = rebuilder.rebuild();
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(newData.data()), newData.size());
    return file.good();
}

const std::vector<uint8_t>& CBDT_CBLC_Font::getFontData() const {
    return fontData;
}

void CBDT_CBLC_Font::setFontData(const std::vector<uint8_t>& data) {
    fontData = data;
}

// Реализация CBDT_CBLC_Rebuilder
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
    appendUInt32(cblcData, 0x00020000);
    appendUInt16(cblcData, static_cast<uint16_t>(strikes.size()));
    
    std::vector<uint32_t> strikeOffsets;
    for (size_t i = 0; i < strikes.size(); ++i) {
        strikeOffsets.push_back(0);
        appendUInt32(cblcData, 0);
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
    appendUInt32(cblcData, 0);
    appendUInt16(cblcData, 72);
    appendUInt16(cblcData, 72);
    
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
    appendUInt16(cblcData, strike.ppem);
    appendUInt16(cblcData, strike.ppem);
    cblcData.push_back(1);
    appendUInt16(cblcData, 0);
    
    size_t arrayOffsetPos = cblcData.size();
    appendUInt32(cblcData, 0);
    
    appendUInt32(cblcData, 1);
    appendUInt32(cblcData, 0);
    
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
    appendUInt32(cblcData, 0);
    
    uint32_t additionalOffset = cblcData.size() - subtableStart;
    setUInt32(cblcData, additionalOffsetPos, additionalOffset);
    
    appendUInt16(cblcData, 1);
    appendUInt16(cblcData, 17);
    
    size_t imageDataOffsetPos = cblcData.size();
    appendUInt32(cblcData, 0);
    
    uint8_t imageSize = 0;
    int8_t bearingX = 0;
    int8_t bearingY = 0;
    uint8_t advance = 0;
    
    for (uint16_t glyphID : strike.glyphIDs) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
            auto it = strike.glyphData.find(glyphID);
            if (it != strike.glyphData.end()) {
                imageSize = std::max(imageSize, static_cast<uint8_t>(it->second.width));
                bearingX = it->second.bearingX;
                bearingY = it->second.bearingY;
                advance = it->second.advance;
                break;
            }
        }
    }
    
    cblcData.push_back(imageSize);
    cblcData.push_back(0);
    cblcData.push_back(static_cast<uint8_t>(bearingX));
    cblcData.push_back(static_cast<uint8_t>(bearingY));
    cblcData.push_back(advance);
    
    uint32_t currentImageOffset = 0;
    for (uint16_t glyphID = firstGlyph; glyphID <= lastGlyph; ++glyphID) {
        if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) != removedGlyphs.end()) {
            appendUInt32(cblcData, 0);
        } else {
            auto it = strike.glyphData.find(glyphID);
            if (it != strike.glyphData.end()) {
                appendUInt32(cblcData, currentImageOffset);
                currentImageOffset += it->second.imageData.size();
            } else {
                appendUInt32(cblcData, 0);
            }
        }
    }
    
    setUInt32(cblcData, imageDataOffsetPos, cblcData.size() - subtableStart + 4);
}

void CBDT_CBLC_Rebuilder::rebuildCBDTTable(std::vector<uint8_t>& cbdtData) {
    appendUInt32(cbdtData, 0x00020000);
    
    for (const auto& strikePair : strikes) {
        for (uint16_t glyphID : strikePair.second.glyphIDs) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphID) == removedGlyphs.end()) {
                auto it = strikePair.second.glyphData.find(glyphID);
                if (it != strikePair.second.glyphData.end()) {
                    cbdtData.insert(cbdtData.end(), 
                                  it->second.imageData.begin(), 
                                  it->second.imageData.end());
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

bool registerCBDTHandler() {
    FontMaster::instance().registerHandler(
        std::make_unique<CBDT_CBLC_Handler>()
    );
    return true;
}

static bool registered = registerCBDTHandler();

} // namespace fontmaster
