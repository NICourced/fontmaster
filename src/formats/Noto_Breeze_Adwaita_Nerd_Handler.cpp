#include "fontmaster/Noto_Breeze_Adwaita_Nerd_Handler.h"
#include "fontmaster/Noto_Breeze_Adwaita_Nerd_Font.h"
#include "fontmaster/NAMEParser.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/FontMaster.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace fontmaster {

bool Noto_Breeze_Adwaita_Nerd_Handler::isNotoFont(const std::string& filepath) const {
    std::string fontFamily = extractFontFamily(filepath);
    std::transform(fontFamily.begin(), fontFamily.end(), fontFamily.begin(), ::tolower);

    // Check if the font family contains "noto" (but not as part of another word like "anton")
    if (fontFamily.find("noto") != std::string::npos) {
        // Additional check to avoid false positives with font names like "anton"
        if (fontFamily == "noto" || fontFamily.find("noto ") == 0 ||
            fontFamily.find("noto-") == 0 || fontFamily.find(" noto") != std::string::npos ||
            fontFamily.find("_noto") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool Noto_Breeze_Adwaita_Nerd_Handler::isNotoEmojiFont(const std::string& filepath) const {
    std::string fontFamily = extractFontFamily(filepath);
    std::transform(fontFamily.begin(), fontFamily.end(), fontFamily.begin(), ::tolower);

    // Check for Noto Emoji variants
    if (fontFamily.find("noto") != std::string::npos &&
        (fontFamily.find("emoji") != std::string::npos ||
         fontFamily.find("color") != std::string::npos ||
         fontFamily.find("emoji") != std::string::npos)) {
        return true;
    }
    return false;
}

bool Noto_Breeze_Adwaita_Nerd_Handler::isBreezeFont(const std::string& filepath) const {
    std::string fontFamily = extractFontFamily(filepath);
    std::transform(fontFamily.begin(), fontFamily.end(), fontFamily.begin(), ::tolower);

    // Check for Breeze font family
    if (fontFamily.find("breeze") != std::string::npos) {
        return true;
    }
    return false;
}

bool Noto_Breeze_Adwaita_Nerd_Handler::isAdwaitaFont(const std::string& filepath) const {
    std::string fontFamily = extractFontFamily(filepath);
    std::transform(fontFamily.begin(), fontFamily.end(), fontFamily.begin(), ::tolower);

    // Check for Adwaita font family
    if (fontFamily.find("adwaita") != std::string::npos) {
        return true;
    }
    return false;
}

bool Noto_Breeze_Adwaita_Nerd_Handler::isNerdFont(const std::string& filepath) const {
    std::string fontFamily = extractFontFamily(filepath);
    std::transform(fontFamily.begin(), fontFamily.end(), fontFamily.begin(), ::tolower);

    // Check for Nerd Font variants (contains "nerd" or ends with "nf")
    if (fontFamily.find("nerd") != std::string::npos || fontFamily.find(" nf") != std::string::npos) {
        return true;
    }
    // Many Nerd Fonts have distinctive names like "Font Awesome", "Devicons", etc.
    // with " Nerd Font" appended
    if (fontFamily.find(" nerd font") != std::string::npos || fontFamily.find("-nf") != std::string::npos) {
        return true;
    }
    return false;
}

std::string Noto_Breeze_Adwaita_Nerd_Handler::extractFontFamily(const std::string& filepath) const {
    try {
        auto fileData = utils::readFontFile(filepath);

        // Parse font tables to find the NAME table
        auto tables = utils::parseTTFTables(fileData);
        const utils::TableRecord* nameRec = utils::findTable(tables, "name");

        if (!nameRec) {
            std::cerr << "No 'name' table found in font: " << filepath << std::endl;
            return "";
        }

        // Extract the font family name from the NAME table
        // Copy the name table data to parse
        std::vector<uint8_t> nameData(
            fileData.begin() + nameRec->offset,
            fileData.begin() + nameRec->offset + nameRec->length
        );

        utils::NAMEParser nameParser(nameData);
        if (nameParser.parse()) {
            std::string fontFamily = nameParser.getFontFamily();
            if (!fontFamily.empty()) {
                return fontFamily;
            }

            // If font family isn't found, try full font name
            std::string fullName = nameParser.getFullName();
            if (!fullName.empty()) {
                return fullName;
            }
        }

        // If NAME parser fails, at least return the filename without extension
        size_t lastSlash = filepath.find_last_of("/\\");
        std::string fileName = (lastSlash == std::string::npos) ? filepath : filepath.substr(lastSlash + 1);

        size_t lastDot = fileName.find_last_of(".");
        if (lastDot != std::string::npos) {
            fileName = fileName.substr(0, lastDot);
        }

        return fileName;
    } catch (const std::exception& e) {
        std::cerr << "Exception extracting font family from " << filepath << ": " << e.what() << std::endl;
        return "";
    }
}

bool Noto_Breeze_Adwaita_Nerd_Handler::canHandle(const std::string& filepath) {
    try {
        auto tables = utils::parseTTFTables(utils::readFontFile(filepath));

        // Check if it's a Noto Emoji font with CBDT/CBLC but defer to CBDT handler
        bool hasCBDT = utils::hasTable(tables, "CBDT");
        bool hasCBLC = utils::hasTable(tables, "CBLC");
        if (hasCBDT && hasCBLC) {
            // Let CBDT/CBLC handler handle this since it's more specialized
            return false;
        }

        // If it's not a CBDT/CBLC font, we can check for Noto fonts
        // Check if this is any of the supported font families
        return isNotoFont(filepath) ||
               isNotoEmojiFont(filepath) ||
               isBreezeFont(filepath) ||
               isAdwaitaFont(filepath) ||
               isNerdFont(filepath);
    } catch (const std::exception& e) {
        std::cerr << "Error in canHandle: " << e.what() << std::endl;
        return false;
    }
}

std::unique_ptr<Font> Noto_Breeze_Adwaita_Nerd_Handler::loadFont(const std::string& filepath) {
    try {
        // Create a new Noto/Breeze/Adwaita/Nerd font instance
        return std::unique_ptr<Font>(new Noto_Breeze_Adwaita_Nerd_Font(filepath));
    } catch (const std::exception& e) {
        std::cerr << "Error in loadFont: " << e.what() << std::endl;
        return nullptr;
    }
}

} // namespace fontmaster
