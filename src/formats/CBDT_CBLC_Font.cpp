#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include "fontmaster/TTFUtils.h"
#include <iostream>
#include <fstream>

namespace fontmaster {

CBDT_CBLC_Font::CBDT_CBLC_Font(const std::string& filepath) 
    : filepath(filepath), parser(std::vector<uint8_t>()) {}

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
    parser = CBDT_CBLC_Parser(fontData);
    
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





} // namespace fontmaster
