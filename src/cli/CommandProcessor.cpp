#include "fontmaster/FontMaster.h"
#include <iostream>
#include <iomanip>

class CommandProcessor {
public:
    static int processList(int argc, char* argv[]) {
        if (argc != 3) {
            std::cerr << "Usage: fontmaster-cli list <fontfile>" << std::endl;
            return 1;
        }
        
        auto font = fontmaster::Font::load(argv[2]);
        if (!font) {
            std::cerr << "Error: Cannot load font " << argv[2] << std::endl;
            return 1;
        }
        
        std::cout << "Font format: ";
        switch (font->getFormat()) {
            case fontmaster::FontFormat::CBDT_CBLC: std::cout << "Google CBDT/CBLC"; break;
            case fontmaster::FontFormat::SBIX: std::cout << "Apple SBIX"; break;
            case fontmaster::FontFormat::COLR_CPAL: std::cout << "Microsoft COLR/CPAL"; break;
            case fontmaster::FontFormat::SVG: std::cout << "Adobe SVG"; break;
            default: std::cout << "Unknown"; break;
        }
        std::cout << std::endl;
        
        auto glyphs = font->listGlyphs();
        std::cout << "Glyphs count: " << glyphs.size() << std::endl;
        
        for (const auto& glyph : glyphs) {
            std::cout << "  " << glyph.name 
                      << " (U+" << std::hex << std::uppercase << std::setw(4) 
                      << std::setfill('0') << glyph.unicode << std::dec << ")"
                      << " - " << glyph.format
                      << " - " << glyph.image_data.size() << " bytes"
                      << std::endl;
        }
        
        return 0;
    }
    
    static int processRemove(int argc, char* argv[]) {
        std::string fontFile;
        std::string glyphName;
        std::string unicodeStr;
        
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--font" && i + 1 < argc) {
                fontFile = argv[++i];
            } else if (arg == "--name" && i + 1 < argc) {
                glyphName = argv[++i];
            } else if (arg == "--unicode" && i + 1 < argc) {
                unicodeStr = argv[++i];
            }
        }
        
        if (fontFile.empty() || (glyphName.empty() && unicodeStr.empty())) {
            std::cerr << "Usage: fontmaster-cli remove --font <file> (--name <name> | --unicode <hex>)" << std::endl;
            return 1;
        }
        
        auto font = fontmaster::Font::load(fontFile);
        if (!font) {
            std::cerr << "Error: Cannot load font " << fontFile << std::endl;
            return 1;
        }
        
        bool success = false;
        if (!glyphName.empty()) {
            success = font->removeGlyph(glyphName);
            std::cout << "Removing glyph by name: " << glyphName << std::endl;
        } else {
            uint32_t unicode = std::stoul(unicodeStr, nullptr, 16);
            success = font->removeGlyph(unicode);
            std::cout << "Removing glyph by Unicode: U+" << unicodeStr << std::endl;
        }
        
        if (success) {
            std::string outputFile = fontFile + ".modified.ttf";
            if (font->save(outputFile)) {
                std::cout << "Success! Modified font saved as: " << outputFile << std::endl;
            } else {
                std::cerr << "Error: Failed to save modified font" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Failed to remove glyph" << std::endl;
            return 1;
        }
        
        return 0;
    }
};
