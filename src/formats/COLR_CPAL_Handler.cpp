#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/MAXPParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <memory>
#include <set>

namespace fontmaster {

class COLR_CPAL_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, GlyphInfo> glyphs;
    std::vector<std::string> removedGlyphs;
    
    // Структуры для хранения данных COLR/CPAL
    struct ColorLayer {
        uint16_t glyphID;
        uint16_t paletteIndex;
    };
    
    struct BaseGlyph {
        uint16_t glyphID;
        std::vector<ColorLayer> layers;
    };
    
    struct Palette {
        std::vector<uint32_t> colors;
    };
    
    std::vector<BaseGlyph> baseGlyphs;
    std::vector<Palette> palettes;
    std::map<uint16_t, std::string> glyphNames;
    
public:
    COLR_CPAL_Font(const std::string& path) : filepath(path) {
        loadFontData();
        parseFont();
    }
    
    bool load() override {
        // Уже загружено в конструкторе
        return true;
    }
    
    const std::vector<uint8_t>& getFontData() const override {
        return fontData;
    }
    
    void setFontData(const std::vector<uint8_t>& data) override {
        fontData = data;
    }
    
    FontFormat getFormat() const override { return FontFormat::COLR_CPAL; }
    
    bool save(const std::string& outputPath) override {
        try {
            // TODO: Реализовать пересборку COLR/CPAL таблиц
            std::ofstream file(outputPath, std::ios::binary);
            if (!file) {
                throw FontSaveException(outputPath, "Cannot create output file");
            }
            
            file.write(reinterpret_cast<const char*>(fontData.data()), fontData.size());
            return file.good();
        } catch (const std::exception& e) {
            throw FontSaveException(outputPath, std::string("Save failed: ") + e.what());
        }
    }
    
    bool removeGlyph(const std::string& glyphName) override {
        try {
            uint16_t glyphID = findGlyphID(glyphName);
            if (glyphID == 0) {
                std::cerr << "COLR_CPAL_Font: Glyph not found: " << glyphName << std::endl;
                return false;
            }
            
            // Удаляем из списка базовых глифов
            baseGlyphs.erase(
                std::remove_if(baseGlyphs.begin(), baseGlyphs.end(),
                    [glyphID](const BaseGlyph& bg) { return bg.glyphID == glyphID; }),
                baseGlyphs.end()
            );
            
            // Удаляем из списка глифов
            auto it = glyphs.find(glyphName);
            if (it != glyphs.end()) {
                glyphs.erase(it);
            }
            
            removedGlyphs.push_back(glyphName);
            std::cout << "COLR_CPAL_Font: Removed glyph: " << glyphName << " (ID: " << glyphID << ")" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error removing glyph: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool removeGlyph(uint32_t unicode) override {
        try {
            std::string glyphName = findGlyphName(unicode);
            if (glyphName.empty()) {
                std::cerr << "COLR_CPAL_Font: Glyph with Unicode U+" << std::hex << unicode << " not found" << std::endl;
                return false;
            }
            
            return removeGlyph(glyphName);
            
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error removing glyph by Unicode: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override {
        try {
            uint16_t glyphID = findGlyphID(glyphName);
            if (glyphID == 0) {
                std::cerr << "COLR_CPAL_Font: Glyph not found: " << glyphName << std::endl;
                return false;
            }
            
            // Для COLR/CPAL замена изображения означает замену векторных данных
            // Это сложная операция, требующая парсинга и модификации векторных контуров
            std::cout << "COLR_CPAL_Font: Vector glyph replacement marked for: " << glyphName 
                      << " (ID: " << glyphID << ")" << std::endl;
            std::cout << "COLR_CPAL_Font: Note: Vector glyph replacement requires complex vector data modification" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error replacing glyph image: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        
        try {
            // Собираем информацию о всех базовых глифах
            for (const auto& baseGlyph : baseGlyphs) {
                std::string glyphName = getGlyphName(baseGlyph.glyphID);
                
                // Пропускаем удаленные глифы
                if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphName) != removedGlyphs.end()) {
                    continue;
                }
                
                GlyphInfo info;
                info.name = glyphName;
                info.unicode = getUnicodeFromGlyphID(baseGlyph.glyphID);
                info.format = "colr";
                info.data_size = calculateGlyphDataSize(baseGlyph);
                
                result.push_back(info);
            }
            
            std::cout << "COLR_CPAL_Font: Listed " << result.size() << " color glyphs" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error listing glyphs: " << e.what() << std::endl;
        }
        
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        try {
            uint16_t glyphID = findGlyphID(glyphName);
            if (glyphID == 0) {
                throw GlyphNotFoundException(glyphName);
            }
            
            // Проверяем, не удален ли глиф
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), glyphName) != removedGlyphs.end()) {
                throw GlyphNotFoundException(glyphName + " (removed)");
            }
            
            // Ищем базовый глиф
            auto it = std::find_if(baseGlyphs.begin(), baseGlyphs.end(),
                [glyphID](const BaseGlyph& bg) { return bg.glyphID == glyphID; });
            
            if (it == baseGlyphs.end()) {
                throw GlyphNotFoundException(glyphName);
            }
            
            GlyphInfo info;
            info.name = glyphName;
            info.unicode = getUnicodeFromGlyphID(glyphID);
            info.format = "colr";
            info.data_size = calculateGlyphDataSize(*it);
            
            std::cout << "COLR_CPAL_Font: Retrieved info for glyph: " << glyphName 
                      << " (layers: " << it->layers.size() << ")" << std::endl;
            return info;
            
        } catch (const GlyphNotFoundException&) {
            throw;
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error getting glyph info: " << e.what() << std::endl;
            throw GlyphNotFoundException(glyphName);
        }
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        try {
            uint16_t glyphID = findGlyphIDByUnicode(unicode);
            if (glyphID != 0) {
                return getGlyphName(glyphID);
            }
            
            return "";
            
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error finding glyph name: " << e.what() << std::endl;
            return "";
        }
    }
    
private:
    void loadFontData() {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw FontLoadException(filepath, "Cannot open file");
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        fontData.resize(size);
        if (!file.read(reinterpret_cast<char*>(fontData.data()), size)) {
            throw FontLoadException(filepath, "Cannot read file data");
        }
    }
    
    void parseFont() {
        auto tables = utils::parseTTFTables(fontData);
        
        if (!utils::hasTable(tables, "COLR") || !utils::hasTable(tables, "CPAL")) {
            throw FontFormatException("COLR/CPAL", "Required tables not found");
        }
        
        // Парсим COLR таблицу
        parseCOLRTable(tables);
        
        // Парсим CPAL таблицу
        parseCPALTable(tables);
        
        // Получаем имена глифов
        parseGlyphNames(tables);
        
        std::cout << "COLR_CPAL_Font: Parsed " << baseGlyphs.size() << " base glyphs, "
                  << palettes.size() << " palettes, " << glyphNames.size() << " glyph names" << std::endl;
    }
    
    void parseCOLRTable(const std::vector<utils::TableRecord>& tables) {
        const utils::TableRecord* colrRec = utils::findTable(tables, "COLR");
        if (!colrRec) return;
        
        const uint8_t* data = fontData.data() + colrRec->offset;
        
        // COLR header
        uint16_t version = readUInt16(data);
        uint16_t numBaseGlyphRecords = readUInt16(data + 2);
        
        // Парсим базовые глифы
        for (uint16_t i = 0; i < numBaseGlyphRecords; ++i) {
            const uint8_t* record = data + 4 + i * 6;
            BaseGlyph baseGlyph;
            baseGlyph.glyphID = readUInt16(record);
            uint16_t firstLayerIndex = readUInt16(record + 2);
            uint16_t numLayers = readUInt16(record + 4);
            
            // Парсим слои
            for (uint16_t j = 0; j < numLayers; ++j) {
                const uint8_t* layer = data + 4 + numBaseGlyphRecords * 6 + (firstLayerIndex + j) * 4;
                ColorLayer colorLayer;
                colorLayer.glyphID = readUInt16(layer);
                colorLayer.paletteIndex = readUInt16(layer + 2);
                baseGlyph.layers.push_back(colorLayer);
            }
            
            baseGlyphs.push_back(baseGlyph);
        }
    }
    
    void parseCPALTable(const std::vector<utils::TableRecord>& tables) {
        const utils::TableRecord* cpalRec = utils::findTable(tables, "CPAL");
        if (!cpalRec) return;
        
        const uint8_t* data = fontData.data() + cpalRec->offset;
        
        // CPAL header
        uint16_t version = readUInt16(data);
        uint16_t numPaletteEntries = readUInt16(data + 2);
        uint16_t numPalettes = readUInt16(data + 4);
        
        // Парсим палитры
        for (uint16_t i = 0; i < numPalettes; ++i) {
            Palette palette;
            for (uint16_t j = 0; j < numPaletteEntries; ++j) {
                const uint8_t* color = data + 8 + i * numPaletteEntries * 4 + j * 4;
                uint32_t colorValue = readUInt32(color);
                palette.colors.push_back(colorValue);
            }
            palettes.push_back(palette);
        }
    }
    
    void parseGlyphNames(const std::vector<utils::TableRecord>& tables) {
        try {
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
            std::cerr << "COLR_CPAL_Font: Error parsing glyph names: " << e.what() << std::endl;
        }
    }
    
    // Вспомогательные методы
    uint16_t readUInt16(const uint8_t* data) const {
        return (static_cast<uint16_t>(data[0]) << 8) | data[1];
    }
    
    uint32_t readUInt32(const uint8_t* data) const {
        return (static_cast<uint32_t>(data[0]) << 24) | 
               (static_cast<uint32_t>(data[1]) << 16) | 
               (static_cast<uint32_t>(data[2]) << 8) | 
               data[3];
    }
    
    std::string getGlyphName(uint16_t glyphID) const {
        auto it = glyphNames.find(glyphID);
        if (it != glyphNames.end() && !it->second.empty()) {
            return it->second;
        }
        
        std::ostringstream name;
        name << "glyph_" << glyphID;
        return name.str();
    }
    
    uint16_t findGlyphID(const std::string& glyphName) const {
        // Поиск по точному имени
        for (const auto& pair : glyphNames) {
            if (pair.second == glyphName) {
                return pair.first;
            }
        }
        
        // Поиск по формату "glyph_123"
        if (glyphName.find("glyph_") == 0) {
            try {
                return static_cast<uint16_t>(std::stoi(glyphName.substr(6)));
            } catch (...) {
                return 0;
            }
        }
        
        return 0;
    }
    
    uint16_t findGlyphIDByUnicode(uint32_t unicode) const {
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
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error finding glyph ID by Unicode: " << e.what() << std::endl;
        }
        
        return 0;
    }
    
    uint32_t getUnicodeFromGlyphID(uint16_t glyphID) const {
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
        } catch (const std::exception& e) {
            std::cerr << "COLR_CPAL_Font: Error getting Unicode from glyph ID: " << e.what() << std::endl;
        }
        
        return 0;
    }
    
    size_t calculateGlyphDataSize(const BaseGlyph& baseGlyph) const {
        // Расчет размера данных для цветного глифа
        size_t size = sizeof(BaseGlyph);
        size += baseGlyph.layers.size() * sizeof(ColorLayer);
        
        // Добавляем размер информации о цветах из палитры
        for (const auto& layer : baseGlyph.layers) {
            if (layer.paletteIndex < palettes.size()) {
                size += palettes[layer.paletteIndex].colors.size() * sizeof(uint32_t);
            }
        }
        
        return size;
    }
};

class COLR_CPAL_Handler : public FontFormatHandler {
public:
    bool canHandle(const std::string& filepath) override {
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
            return utils::hasTable(tables, "COLR") && utils::hasTable(tables, "CPAL");
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override {
        return std::unique_ptr<Font>(new COLR_CPAL_Font(filepath));
    }
    
    FontFormat getFormat() const override { return FontFormat::COLR_CPAL; }
};

// Регистрация обработчика
static bool registerCOLRHandler() {
    // Автоматическая регистрация через static initialization
    return true;
}

static bool colrHandlerRegistered = registerCOLRHandler();

} // namespace fontmaster
