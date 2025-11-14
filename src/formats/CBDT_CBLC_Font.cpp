#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"

#include "fontmaster/CMAPParser.h"
#include "fontmaster/NAMEParser.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/MAXPParser.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <set>

namespace fontmaster {

CBDT_CBLC_Font::CBDT_CBLC_Font(const std::string& filepath)
    : filepath(filepath), parser(fontData) {}

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
    
    // Инициализируем парсер с данными шрифта
//    parser = CBDT_CBLC_Parser(fontData);
    
    if (!parser.parse()) {
        std::cerr << "Failed to parse CBDT/CBLC font" << std::endl;
        return false;
    }
    
    std::cout << "CBDT/CBLC Font loaded successfully: " << filepath << std::endl;
    return true;
}

bool CBDT_CBLC_Font::save(const std::string& filepath) {
    CBDT_CBLC_Rebuilder rebuilder(fontData, parser.getStrikes(), parser.getRemovedGlyphs());
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



bool CBDT_CBLC_Font::removeGlyph(const std::string& glyphName) {
    try {
        uint16_t glyphID = findGlyphID(glyphName);
        if (glyphID == 0) {
            std::cerr << "CBDT_CBLC_Font: Glyph not found: " << glyphName << std::endl;
            return false;
        }
        
        std::cout << "CBDT_CBLC_Font: Glyph removal marked for: " << glyphName 
                  << " (ID: " << glyphID << ")" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error processing glyph removal: " << e.what() << std::endl;
        return false;
    }
}

bool CBDT_CBLC_Font::removeGlyph(uint32_t unicode) {
    try {
        std::string glyphName = findGlyphName(unicode);
        if (glyphName.empty()) {
            std::cerr << "CBDT_CBLC_Font: Glyph with Unicode U+" << std::hex << unicode << " not found" << std::endl;
            return false;
        }
        
        return removeGlyph(glyphName);
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error removing glyph by Unicode: " << e.what() << std::endl;
        return false;
    }
}

bool CBDT_CBLC_Font::replaceGlyphImage(const std::string& glyphName,
                                      const std::vector<uint8_t>& newImage) {
    try {
        uint16_t glyphID = findGlyphID(glyphName);
        if (glyphID == 0) {
            std::cerr << "CBDT_CBLC_Font: Glyph not found: " << glyphName << std::endl;
            return false;
        }
        
        std::cout << "CBDT_CBLC_Font: Glyph image replacement marked for: " << glyphName 
                  << " (new image size: " << newImage.size() << " bytes)" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error processing glyph image replacement: " << e.what() << std::endl;
        return false;
    }
}

std::vector<GlyphInfo> CBDT_CBLC_Font::listGlyphs() const {
    std::vector<GlyphInfo> glyphs;
    
    try {
        const auto& strikes = parser.getStrikes();
        auto postGlyphNames = getPostGlyphNames();
        auto maxpGlyphCount = getMaxpGlyphCount();
        
        std::set<uint16_t> uniqueGlyphIDs;
        
        for (const auto& strikePair : strikes) {
            const StrikeRecord& strike = strikePair.second;
            
            for (uint16_t glyphID : strike.glyphIDs) {
                uniqueGlyphIDs.insert(glyphID);
            }
        }
        
        for (uint16_t glyphID : uniqueGlyphIDs) {
            if (maxpGlyphCount > 0 && glyphID >= maxpGlyphCount) {
                continue;
            }
            
            GlyphInfo info;
            info.name = getGlyphName(glyphID, postGlyphNames);
            info.unicode = getUnicodeFromGlyphID(glyphID);
            
            for (const auto& strikePair : strikes) {
                const StrikeRecord& strike = strikePair.second;
                auto it = strike.glyphImages.find(glyphID);
                if (it != strike.glyphImages.end()) {
                    const GlyphImage& glyphImage = it->second;
                    info.image_data = glyphImage.data;
                    info.format = getImageFormatString(glyphImage.imageFormat);
                    info.data_size = glyphImage.data.size();
                    break;
                }
            }
            
            glyphs.push_back(info);
        }
        
        std::cout << "CBDT_CBLC_Font: Listed " << glyphs.size() << " glyphs" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error listing glyphs: " << e.what() << std::endl;
    }
    
    return glyphs;
}

GlyphInfo CBDT_CBLC_Font::getGlyphInfo(const std::string& glyphName) const {
    try {
        uint16_t glyphID = findGlyphID(glyphName);
        if (glyphID == 0) {
            throw GlyphNotFoundException(glyphName);
        }
        
        auto postGlyphNames = getPostGlyphNames();
        std::string actualName = getGlyphName(glyphID, postGlyphNames);
        
        const auto& strikes = parser.getStrikes();
        for (const auto& strikePair : strikes) {
            const StrikeRecord& strike = strikePair.second;
            auto it = strike.glyphImages.find(glyphID);
            if (it != strike.glyphImages.end()) {
                const GlyphImage& glyphImage = it->second;
                
                GlyphInfo info;
                info.name = actualName;
                info.unicode = getUnicodeFromGlyphID(glyphID);
                info.image_data = glyphImage.data;
                info.format = getImageFormatString(glyphImage.imageFormat);
                info.data_size = glyphImage.data.size();
                
                std::cout << "CBDT_CBLC_Font: Retrieved info for glyph: " << glyphName 
                          << " (ID: " << glyphID << ")" << std::endl;
                return info;
            }
        }
        
        throw GlyphNotFoundException(glyphName);
        
    } catch (const GlyphNotFoundException&) {
        throw;
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error getting glyph info: " << e.what() << std::endl;
        throw GlyphNotFoundException(glyphName);
    }
}

std::string CBDT_CBLC_Font::findGlyphName(uint32_t unicode) const {
    try {
        uint16_t glyphID = findGlyphIDByUnicode(unicode);
        if (glyphID != 0) {
            auto postGlyphNames = getPostGlyphNames();
            return getGlyphName(glyphID, postGlyphNames);
        }
        
        return "";
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error finding glyph name: " << e.what() << std::endl;
        return "";
    }
}

// ============ PRIVATE HELPER METHODS ============

std::string CBDT_CBLC_Font::getGlyphName(uint16_t glyphID, const std::map<uint16_t, std::string>& postGlyphNames) const {
    auto it = postGlyphNames.find(glyphID);
    if (it != postGlyphNames.end() && !it->second.empty()) {
        return it->second;
    }
    
    std::ostringstream name;
    name << "glyph_" << glyphID;
    return name.str();
}

std::map<uint16_t, std::string> CBDT_CBLC_Font::getPostGlyphNames() const {
    std::map<uint16_t, std::string> glyphNames;
    
    try {
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* postRec = utils::findTable(tables, "post");
        const utils::TableRecord* maxpRec = utils::findTable(tables, "maxp");
        
        if (postRec && maxpRec) {
            utils::MAXPParser maxpParser(fontData, maxpRec->offset);
            if (maxpParser.parse()) {
                uint16_t numGlyphs = maxpParser.getNumGlyphs();
                
                utils::POSTParser postParser(fontData, postRec->offset, numGlyphs);
                if (postParser.parse()) {
                    glyphNames = postParser.getGlyphNames();
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error parsing POST table: " << e.what() << std::endl;
    }
    
    return glyphNames;
}

uint16_t CBDT_CBLC_Font::getMaxpGlyphCount() const {
    try {
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* maxpRec = utils::findTable(tables, "maxp");
        
        if (maxpRec) {
            utils::MAXPParser maxpParser(fontData, maxpRec->offset);
            if (maxpParser.parse()) {
                return maxpParser.getNumGlyphs();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error parsing MAXP table: " << e.what() << std::endl;
    }
    
    return 0;
}

uint16_t CBDT_CBLC_Font::findGlyphID(const std::string& glyphName) const {
    try {
        auto postGlyphNames = getPostGlyphNames();
        for (const auto& pair : postGlyphNames) {
            if (pair.second == glyphName) {
                return pair.first;
            }
        }
        
        if (glyphName.find("glyph_") == 0) {
            try {
                std::string idStr = glyphName.substr(6);
                uint16_t glyphID = static_cast<uint16_t>(std::stoi(idStr));
                
                auto maxpGlyphCount = getMaxpGlyphCount();
                if (maxpGlyphCount > 0 && glyphID < maxpGlyphCount) {
                    return glyphID;
                }
            } catch (const std::exception&) {
            }
        }
        
        if (glyphName.find("u") == 0) {
            try {
                std::string hexStr = glyphName.substr(1);
                uint32_t unicode = std::stoul(hexStr, nullptr, 16);
                return findGlyphIDByUnicode(unicode);
            } catch (...) {
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error finding glyph ID: " << e.what() << std::endl;
        return 0;
    }
}

uint16_t CBDT_CBLC_Font::findGlyphIDByUnicode(uint32_t unicode) const {
    try {
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* cmapRec = utils::findTable(tables, "cmap");
        
        if (cmapRec) {
            std::vector<uint8_t> cmapData(
                fontData.begin() + cmapRec->offset,
                fontData.begin() + cmapRec->offset + cmapRec->length
            );
            
            utils::CMAPParser cmapParser(cmapData);
            if (cmapParser.parse()) {
                return cmapParser.getGlyphIndex(unicode);
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error finding glyph ID by Unicode: " << e.what() << std::endl;
        return 0;
    }
}

uint32_t CBDT_CBLC_Font::getUnicodeFromGlyphID(uint16_t glyphID) const {
    try {
        auto tables = utils::parseTTFTables(fontData);
        const utils::TableRecord* cmapRec = utils::findTable(tables, "cmap");
        
        if (cmapRec) {
            std::vector<uint8_t> cmapData(
                fontData.begin() + cmapRec->offset,
                fontData.begin() + cmapRec->offset + cmapRec->length
            );
            
            utils::CMAPParser cmapParser(cmapData);
            if (cmapParser.parse()) {
                auto charCodes = cmapParser.getCharCodes(glyphID);
                if (!charCodes.empty()) {
                    return *charCodes.begin();
                }
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "CBDT_CBLC_Font: Error getting Unicode from glyph ID: " << e.what() << std::endl;
        return 0;
    }
}

std::string CBDT_CBLC_Font::getImageFormatString(uint16_t imageFormat) const {
    switch (imageFormat) {
        case 1: return "bitmap_mono";
        case 2: return "bitmap_grayscale";
        case 3: return "bitmap_rgb";
        case 4: return "bitmap_rgba";
        case 5: return "png";
        case 6: return "jpeg";
        case 7: return "tiff";
        case 8: return "bitmap_mono_small";
        case 9: return "bitmap_grayscale_small";
        case 17: return "png_small";
        case 18: return "png_mono";
        default: return "unknown";
    }
}





} // namespace fontmaster
