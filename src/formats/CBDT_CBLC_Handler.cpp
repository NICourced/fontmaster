#include "fontmaster/CBDT_CBLC_Handler.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/FontMasterImpl.h"
#include "fontmaster/TTFUtils.h"
#include <iostream>
#include <fstream>

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

} // namespace fontmaster
