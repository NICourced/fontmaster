#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <fstream>

namespace fontmaster {

class SBIX_Handler : public FontFormatHandler {
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
            return utils::hasTable(tables, "sbix");
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override {
        return std::make_unique<SBIX_Font>(filepath);
    }
    
    FontFormat getFormat() const override { return FontFormat::SBIX; }
};

class SBIX_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, std::vector<uint8_t>> glyphImages;
    std::vector<std::string> removedGlyphs;
    
public:
    SBIX_Font(const std::string& path) : filepath(path) {
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
        if (!utils::hasTable(tables, "sbix")) {
            throw FontFormatException("SBIX", "sbix table not found");
        }
        
        // Упрощенный парсинг для демонстрации
        std::cout << "SBIX table found" << std::endl;
        
        // Заглушка для демонстрации
        for (int i = 0; i < 10; ++i) {
            std::string glyphName = "u1F6" + std::to_string(i) + "0";
            glyphImages[glyphName] = {0x89, 0x50, 0x4E, 0x47}; // PNG header
        }
    }
    
public:
    FontFormat getFormat() const override { return FontFormat::SBIX; }
    
    bool removeGlyph(const std::string& glyphName) override {
        auto it = glyphImages.find(glyphName);
        if (it == glyphImages.end()) {
            return false;
        }
        
        glyphImages.erase(it);
        removedGlyphs.push_back(glyphName);
        return true;
    }
    
    bool removeGlyph(uint32_t unicode) override {
        std::string name = findGlyphName(unicode);
        if (name.empty()) return false;
        return removeGlyph(name);
    }
    
    bool replaceGlyphImage(const std::string& glyphName,
                          const std::vector<uint8_t>& newImage) override {
        auto it = glyphImages.find(glyphName);
        if (it == glyphImages.end()) {
            return false;
        }
        
        it->second = newImage;
        return true;
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        for (const auto& pair : glyphImages) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), pair.first) != removedGlyphs.end()) {
                continue;
            }
            
            GlyphInfo info;
            info.name = pair.first;
            info.format = "png";
            info.image_data = pair.second;
            info.data_size = pair.second.size();
            result.push_back(info);
        }
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        auto it = glyphImages.find(glyphName);
        if (it != glyphImages.end()) {
            GlyphInfo info;
            info.name = glyphName;
            info.format = "png";
            info.image_data = it->second;
            info.data_size = it->second.size();
            return info;
        }
        throw GlyphNotFoundException(glyphName);
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        std::string expectedName = "u" + std::to_string(unicode);
        return glyphImages.count(expectedName) ? expectedName : "";
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

static bool registerSBIXHandler() {
    FontMasterImpl::instance().registerHandler(
        std::make_unique<SBIX_Handler>()
    );
    return true;
}

static bool sbixHandlerRegistered = registerSBIXHandler();

} // namespace fontmaster
