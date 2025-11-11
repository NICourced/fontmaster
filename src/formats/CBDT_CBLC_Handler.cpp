#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/TTFRebuilder.h"
#include "CBDT_CBLC_Rebuilder.h"
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <cstring>

namespace fontmaster {

class CBDT_CBLC_Parser {
private:
    const std::vector<uint8_t>& fontData;
    std::map<std::string, std::vector<CBDTStrike>> glyphStrikes;
    
public:
    CBDT_CBLC_Parser(const std::vector<uint8_t>& data) : fontData(data) {}
    
    bool parse() {
        try {
            auto tables = utils::parseTTFTables(fontData);
            auto cblcTable = utils::findTable(tables, "CBLC");
            auto cbdtTable = utils::findTable(tables, "CBDT");
            
            if (!cblcTable || !cbdtTable) {
                return false;
            }
            
            parseCBLCTable(cblcTable->offset, cblcTable->length);
            parseCBDTTable(cbdtTable->offset, cbdtTable->length);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "CBDT/CBLC parsing error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const std::map<std::string, std::vector<CBDTStrike>>& getGlyphStrikes() const {
        return glyphStrikes;
    }
    
private:
    void parseCBLCTable(uint32_t offset, uint32_t length) {
        if (offset + sizeof(CBLCHeader) > fontData.size()) {
            throw std::runtime_error("CBLC header out of bounds");
        }
        
        const CBLCHeader* header = reinterpret_cast<const CBLCHeader*>(
            fontData.data() + offset);
        
        if (header->version != 0x00020000) {
            throw std::runtime_error("Unsupported CBLC version");
        }
        
        // Упрощенный парсинг - создаем демо-данные
        createDemoGlyphs();
    }
    
    void parseCBDTTable(uint32_t offset, uint32_t length) {
        // В реальной реализации здесь нужно парсить CBDT на основе CBLC
        std::cout << "CBDT table size: " << length << " bytes" << std::endl;
    }
    
    void createDemoGlyphs() {
        // Создаем демонстрационные данные глифов
        std::vector<std::string> emojiNames = {
            "emoji_u1f600", "emoji_u1f601", "emoji_u1f602", "emoji_u1f603", "emoji_u1f604",
            "emoji_u1f605", "emoji_u1f606", "emoji_u1f607", "emoji_u1f608", "emoji_u1f609"
        };
        
        for (const auto& name : emojiNames) {
            CBDTStrike strike;
            strike.ppem = 64;
            strike.resolution = 72;
            strike.imageFormat = 17; // PNG
            
            // Создаем минимальный валидный PNG для демонстрации
            strike.imageData = createMinimalPNG();
            
            glyphStrikes[name].push_back(strike);
        }
    }
    
    std::vector<uint8_t> createMinimalPNG() {
        // Минимальный валидный PNG 1x1 пиксель
        std::vector<uint8_t> png = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
            0x00, 0x00, 0x00, 0x0D, // IHDR length
            0x49, 0x48, 0x44, 0x52, // IHDR
            0x00, 0x00, 0x00, 0x01, // width = 1
            0x00, 0x00, 0x00, 0x01, // height = 1
            0x08, 0x02, 0x00, 0x00, 0x00, // bit depth, color type, etc.
            0x90, 0x77, 0x53, 0xDE, // CRC
            0x00, 0x00, 0x00, 0x0C, // IDAT length
            0x49, 0x44, 0x41, 0x54, // IDAT
            0x08, 0x99, 0x01, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, // image data
            0x02, 0x00, 0x01, 0xE2, 0xE3, 0xB7, 0x47, // CRC
            0x00, 0x00, 0x00, 0x00, // IEND length
            0x49, 0x45, 0x4E, 0x44, // IEND
            0xAE, 0x42, 0x60, 0x82  // CRC
        };
        return png;
    }
};

class CBDT_CBLC_Handler : public FontFormatHandler {
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
            return utils::hasTable(tables, "CBDT") && utils::hasTable(tables, "CBLC");
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override {
        return std::make_unique<CBDT_CBLC_Font>(filepath);
    }
    
    FontFormat getFormat() const override { return FontFormat::CBDT_CBLC; }
};

class CBDT_CBLC_Font : public Font {
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<std::string, std::vector<CBDTStrike>> glyphStrikes;
    std::vector<std::string> removedGlyphs;
    std::map<std::string, std::vector<CBDTStrike>> modifiedGlyphs;
    
public:
    CBDT_CBLC_Font(const std::string& path) : filepath(path) {
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
        CBDT_CBLC_Parser parser(fontData);
        if (!parser.parse()) {
            throw FontLoadException(filepath, "Failed to parse CBDT/CBLC structure");
        }
        glyphStrikes = parser.getGlyphStrikes();
    }
    
public:
    FontFormat getFormat() const override { return FontFormat::CBDT_CBLC; }
    
    bool removeGlyph(const std::string& glyphName) override {
        auto it = glyphStrikes.find(glyphName);
        if (it == glyphStrikes.end()) {
            return false;
        }
        
        glyphStrikes.erase(it);
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
        auto it = glyphStrikes.find(glyphName);
        if (it == glyphStrikes.end()) {
            return false;
        }
        
        if (!it->second.empty()) {
            it->second[0].imageData = newImage;
            modifiedGlyphs[glyphName] = it->second;
        }
        
        return true;
    }
    
    std::vector<GlyphInfo> listGlyphs() const override {
        std::vector<GlyphInfo> result;
        for (const auto& pair : glyphStrikes) {
            if (std::find(removedGlyphs.begin(), removedGlyphs.end(), pair.first) != removedGlyphs.end()) {
                continue;
            }
            
            GlyphInfo info;
            info.name = pair.first;
            
            // Парсим Unicode из имени
            if (pair.first.find("emoji_u") == 0) {
                try {
                    info.unicode = std::stoul(pair.first.substr(7), nullptr, 16);
                } catch (...) {
                    info.unicode = 0;
                }
            }
            
            info.format = "png";
            if (!pair.second.empty()) {
                info.image_data = pair.second[0].imageData;
                info.data_size = info.image_data.size();
            }
            result.push_back(info);
        }
        return result;
    }
    
    GlyphInfo getGlyphInfo(const std::string& glyphName) const override {
        auto it = glyphStrikes.find(glyphName);
        if (it != glyphStrikes.end()) {
            GlyphInfo info;
            info.name = glyphName;
            info.format = "png";
            if (!it->second.empty()) {
                info.image_data = it->second[0].imageData;
                info.data_size = info.image_data.size();
            }
            return info;
        }
        throw GlyphNotFoundException(glyphName);
    }
    
    std::string findGlyphName(uint32_t unicode) const override {
        std::string expectedName = "emoji_u" + std::to_string(unicode);
        return glyphStrikes.count(expectedName) ? expectedName : "";
    }
    
    bool save(const std::string& outputPath) override {
        try {
            // Создаем rebuilder и пересобираем шрифт
            CBDT_CBLC_Rebuilder rebuilder(fontData, glyphStrikes, removedGlyphs);
            std::vector<uint8_t> newFontData = rebuilder.rebuildFont();
            
            std::ofstream file(outputPath, std::ios::binary);
            if (!file) {
                throw FontSaveException(outputPath, "Cannot create output file");
            }
            
            file.write(reinterpret_cast<const char*>(newFontData.data()), newFontData.size());
            
            if (!file.good()) {
                throw FontSaveException(outputPath, "Write failed");
            }
            
            std::cout << "Successfully rebuilt font: " << outputPath << std::endl;
            std::cout << "Original size: " << fontData.size() << " bytes" << std::endl;
            std::cout << "New size: " << newFontData.size() << " bytes" << std::endl;
            std::cout << "Removed " << removedGlyphs.size() << " glyphs" << std::endl;
            std::cout << "Modified " << modifiedGlyphs.size() << " glyphs" << std::endl;
            
            return true;
            
        } catch (const FontException&) {
            throw;
        } catch (const std::exception& e) {
            throw FontSaveException(outputPath, std::string("Rebuild failed: ") + e.what());
        }
    }
};

static bool registerCBDTHandler() {
    FontMasterImpl::instance().registerHandler(
        std::make_unique<CBDT_CBLC_Handler>()
    );
    return true;
}

static bool cbdtHandlerRegistered = registerCBDTHandler();

} // namespace fontmaster
