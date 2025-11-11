#include "fontmaster/FontMaster.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <fstream>

class CommandProcessor {
public:
    static int processList(int argc, char* argv[]) {
        if (argc != 3) {
            std::cerr << "Usage: fontmaster-cli list <fontfile>" << std::endl;
            return 1;
        }
        
        try {
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
                std::cout << "  " << glyph.name;
                if (glyph.unicode != 0) {
                    std::cout << " (U+" << std::hex << std::uppercase << std::setw(4) 
                              << std::setfill('0') << glyph.unicode << std::dec << ")";
                }
                std::cout << " - " << glyph.format;
                std::cout << " - " << glyph.data_size << " bytes";
                std::cout << std::endl;
            }
            
            return 0;
        } catch (const fontmaster::FontException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    static int processRemove(int argc, char* argv[]) {
        std::string fontFile;
        std::string glyphName;
        std::string unicodeStr;
        std::string outputFile;
        
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--font" && i + 1 < argc) {
                fontFile = argv[++i];
            } else if (arg == "--name" && i + 1 < argc) {
                glyphName = argv[++i];
            } else if (arg == "--unicode" && i + 1 < argc) {
                unicodeStr = argv[++i];
            } else if (arg == "--output" && i + 1 < argc) {
                outputFile = argv[++i];
            }
        }
        
        if (fontFile.empty() || (glyphName.empty() && unicodeStr.empty())) {
            std::cerr << "Usage: fontmaster-cli remove --font <file> (--name <name> | --unicode <hex>) [--output <file>]" << std::endl;
            return 1;
        }
        
        if (outputFile.empty()) {
            outputFile = fontFile + ".modified.ttf";
        }
        
        try {
            auto font = fontmaster::Font::load(fontFile);
            if (!font) {
                std::cerr << "Error: Cannot load font " << fontFile << std::endl;
                return 1;
            }
            
            bool success = false;
            if (!glyphName.empty()) {
                std::cout << "Removing glyph by name: " << glyphName << std::endl;
                success = font->removeGlyph(glyphName);
            } else {
                uint32_t unicode = std::stoul(unicodeStr, nullptr, 16);
                std::cout << "Removing glyph by Unicode: U+" << unicodeStr << std::endl;
                success = font->removeGlyph(unicode);
            }
            
            if (success) {
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
        } catch (const fontmaster::FontException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    static int processReplace(int argc, char* argv[]) {
        std::string fontFile;
        std::string glyphName;
        std::string imageFile;
        std::string outputFile;
        
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--font" && i + 1 < argc) {
                fontFile = argv[++i];
            } else if (arg == "--name" && i + 1 < argc) {
                glyphName = argv[++i];
            } else if (arg == "--image" && i + 1 < argc) {
                imageFile = argv[++i];
            } else if (arg == "--output" && i + 1 < argc) {
                outputFile = argv[++i];
            }
        }
        
        if (fontFile.empty() || glyphName.empty() || imageFile.empty()) {
            std::cerr << "Usage: fontmaster-cli replace --font <file> --name <name> --image <file> [--output <file>]" << std::endl;
            return 1;
        }
        
        if (outputFile.empty()) {
            outputFile = fontFile + ".modified.ttf";
        }
        
        try {
            // Читаем файл изображения
            std::ifstream image(imageFile, std::ios::binary);
            if (!image) {
                std::cerr << "Error: Cannot open image file " << imageFile << std::endl;
                return 1;
            }
            
            image.seekg(0, std::ios::end);
            size_t size = image.tellg();
            image.seekg(0, std::ios::beg);
            
            std::vector<uint8_t> imageData(size);
            image.read(reinterpret_cast<char*>(imageData.data()), size);
            
            auto font = fontmaster::Font::load(fontFile);
            if (!font) {
                std::cerr << "Error: Cannot load font " << fontFile << std::endl;
                return 1;
            }
            
            std::cout << "Replacing image for glyph: " << glyphName << std::endl;
            if (font->replaceGlyphImage(glyphName, imageData)) {
                if (font->save(outputFile)) {
                    std::cout << "Success! Modified font saved as: " << outputFile << std::endl;
                } else {
                    std::cerr << "Error: Failed to save modified font" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Failed to replace glyph image" << std::endl;
                return 1;
            }
            
            return 0;
        } catch (const fontmaster::FontException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    static int processInfo(int argc, char* argv[]) {
        if (argc != 3) {
            std::cerr << "Usage: fontmaster-cli info <fontfile>" << std::endl;
            return 1;
        }
        
        try {
            auto font = fontmaster::Font::load(argv[2]);
            if (!font) {
                std::cerr << "Error: Cannot load font " << argv[2] << std::endl;
                return 1;
            }
            
            std::cout << "Font Information:" << std::endl;
            std::cout << "  Format: ";
            switch (font->getFormat()) {
                case fontmaster::FontFormat::CBDT_CBLC: std::cout << "Google CBDT/CBLC"; break;
                case fontmaster::FontFormat::SBIX: std::cout << "Apple SBIX"; break;
                case fontmaster::FontFormat::COLR_CPAL: std::cout << "Microsoft COLR/CPAL"; break;
                case fontmaster::FontFormat::SVG: std::cout << "Adobe SVG"; break;
                default: std::cout << "Unknown"; break;
            }
            std::cout << std::endl;
            
            auto glyphs = font->listGlyphs();
            std::cout << "  Glyph count: " << glyphs.size() << std::endl;
            
            size_t totalSize = 0;
            for (const auto& glyph : glyphs) {
                totalSize += glyph.data_size;
            }
            std::cout << "  Total image data: " << totalSize << " bytes" << std::endl;
            
            return 0;
        } catch (const fontmaster::FontException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
};

void printUsage() {
    std::cout << "FontMaster - Universal Emoji Font Tool" << std::endl;
    std::cout << "Usage: fontmaster-cli <command> [options]" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  list <fontfile>                                 List all glyphs" << std::endl;
    std::cout << "  remove --font <font> --name <name>              Remove glyph by name" << std::endl;
    std::cout << "  remove --font <font> --unicode <hex>            Remove glyph by unicode" << std::endl;
    std::cout << "  replace --font <font> --name <name> --image <file>  Replace glyph image" << std::endl;
    std::cout << "  info <fontfile>                                 Show font information" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported formats: CBDT/CBLC (Google), SBIX (Apple), COLR/CPAL (Microsoft), SVG (Adobe)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    
    std::string command = argv[1];
    
    try {
        if (command == "list") {
            return CommandProcessor::processList(argc, argv);
        } else if (command == "remove") {
            return CommandProcessor::processRemove(argc, argv);
        } else if (command == "replace") {
            return CommandProcessor::processReplace(argc, argv);
        } else if (command == "info") {
            return CommandProcessor::processInfo(argc, argv);
        } else if (command == "help" || command == "--help" || command == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printUsage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
