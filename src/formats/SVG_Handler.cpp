#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"

namespace fontmaster {

class SVG_Handler : public FontFormatHandler {
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
            return utils::hasTable(tables, "SVG ");
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override {
        return std::make_unique<SVG_Font>(filepath);
    }
    
    FontFormat getFormat() const override { return FontFormat::SVG; }
};

class SVG_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, std::string> glyphSVG;
    std::vector<std::string> removedGlyphs;
    
public:
    SVG_Font(const std::string& path) : filepath(path) {
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
        if (!utils::hasTable(tables, "SVG ")) {
            throw FontFormatException("SVG", "SVG table not found");
        }
        
        std::cout << "SVG table found" << std::endl;
        
        // Заглушка для демонстрации
        for (int i = 0; i < 10; ++i) {
            std::string glyphName = "svg_glyph_" + std::to_string(i);
            glyphSVG[glyphName] = "<svg><circle cx='50' cy='50' r='40'/></svg>";
        }
    }
    
public:
    FontFormat getFormat() const override { return FontFormat::SVG; }
    
    bool removeGlyph(const std::string& glyphName) override {
        auto it = glyphSVG.find(glyphName);
        if (it == glyphSVG.end()) {
            return false;
        }
        
        glyphSVG.erase(it);
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
        auto it = glyphSVG.find(glyphName);
        if (it == glyphSVG.end()) {
            return false;
        }
        
        std::string newSVG(newImage.begin(), newImage.end());
        it->second = newSVG;
        return true;
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        for (const auto& pair : glyphSVG) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), pair.first) != removedGlyphs.end()) {
                continue;
            }
            
            GlyphInfo info;
            info.name = pair.first;
            info.format = "svg";
            const std::string& svg = pair.second;
            info.image_data.assign(svg.begin(), svg.end());
            info.data_size = svg.size();
            result.push_back(info);
        }
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        auto it = glyphSVG.find(glyphName);
        if (it != glyphSVG.end()) {
            GlyphInfo info;
            info.name = glyphName;
            info.format = "svg";
            const std::string& svg = it->second;
            info.image_data.assign(svg.begin(), svg.end());
            info.data_size = svg.size();
            return info;
        }
        throw GlyphNotFoundException(glyphName);
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        // Для SVG нужно парсить cmap таблицу
        // Временно возвращаем пустую строку
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

static bool registerSVGHandler() {
    FontMasterImpl::instance().registerHandler(
        std::make_unique<SVG_Handler>()
    );
    return true;
}

static bool svgHandlerRegistered = registerSVGHandler();

} // namespace fontmaster
