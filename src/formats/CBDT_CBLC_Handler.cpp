#include "fontmaster/CBDT_CBLC_Handler.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <iostream>
#include <fstream>

namespace fontmaster {

bool CBDT_CBLC_Handler::canHandle(const std::string& filepath) {
    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
            
        if (size < 1024) return false;
            
        std::vector<uint8_t> header(1024);
        file.read(reinterpret_cast<char*>(header.data()), header.size());
            
        auto tables = utils::parseTTFTables(header);
        bool hasCBDT = utils::hasTable(tables, "CBDT");
        bool hasCBLC = utils::hasTable(tables, "CBLC");
        
        return hasCBDT && hasCBLC;
    } catch (...) {
        return false;
    }
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
    
    if (!canHandle(filepath)) {
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

} // namespace fontmaster
