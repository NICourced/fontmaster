#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"

namespace fontmaster {

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
        return std::make_unique<COLR_CPAL_Font>(filepath);
    }
    
    FontFormat getFormat() const override { return FontFormat::COLR_CPAL; }
};

class COLR_CPAL_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, GlyphInfo> colorGlyphs;
    std::vector<std::string> removedGlyphs;
    
public:
    COLR_CPAL_Font(const std::string& path) : filepath(path) {
        loadFontData();
        parseFont();
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
        
        std::cout << "COLR/CPAL tables found" << std::endl;
        
        // Заглушка для демонстрации
        for (int i = 0; i < 10; ++i) {
            std::string glyphName = "colr_glyph_" + std::to_string(i);
            GlyphInfo info;
            info.name = glyphName;
            info.unicode = 0x1F600 + i;
            info.format = "colr";
            info.data_size = 0;
            colorGlyphs[glyphName] = info;
        }
    }
    
public:
    FontFormat getFormat() const override { return FontFormat::COLR_CPAL; }
    
    bool removeGlyph(const std::string& glyphName) override {
        auto it = colorGlyphs.find(glyphName);
        if (it == colorGlyphs.end()) {
            return false;
        }
        
        colorGlyphs.erase(it);
        removedGlyphs.push_back(glyphName);
        return true;
    }
    
    bool removeGlyph(uint32_t unicode) override {
        for (const auto& pair : colorGlyphs) {
            if (pair.second.unicode == unicode) {
                return removeGlyph(pair.first);
            }
        }
        return false;
    }
    
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override {
        // COLR/CPAL использует векторные слои, а не растровые изображения
        // Замена сложнее и требует модификации векторных данных
        std::cerr << "Warning: Image replacement for COLR/CPAL fonts is not fully supported" << std::endl;
        return false;
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        for (const auto& pair : colorGlyphs) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), pair.first) != removedGlyphs.end()) {
                continue;
            }
            result.push_back(pair.second);
        }
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        auto it = colorGlyphs.find(glyphName);
        if (it != colorGlyphs.end()) {
            return it->second;
        }
        throw GlyphNotFoundException(glyphName);
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        for (const auto& pair : colorGlyphs) {
            if (pair.second.unicode == unicode) {
                return pair.first;
            }
        }
        return "";
    }
    
    bool save(const std::string& outputPath) override {
        try {
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
};

static bool registerCOLRHandler() {
    FontMasterImpl::instance().registerHandler(
        std::make_unique<COLR_CPAL_Handler>()
    );
    return true;
}

static bool colrHandlerRegistered = registerCOLRHandler();

} // namespace fontmaster
