#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <iostream>

namespace fontmaster {

enum class FontFormat {
    UNKNOWN,
    CBDT_CBLC,  // Google
    SBIX,       // Apple  
    COLR_CPAL,  // Microsoft
    SVG,        // Adobe
    STANDARD    // Обычный шрифт
};

struct GlyphInfo {
    std::string name;
    uint32_t unicode;
    std::vector<uint8_t> image_data;
    std::string format; // "png", "svg", "colr"
    size_t data_size;
};

struct CBDTStrike {
    uint32_t ppem;
    uint32_t resolution;
    std::vector<uint8_t> imageData;
    uint16_t imageFormat;
};

class Font {
public:
    virtual ~Font() = default;
    
    static std::unique_ptr<Font> load(const std::string& filepath);
    
    virtual FontFormat getFormat() const = 0;
    virtual bool save(const std::string& filepath) = 0;
    
    // Основные операции
    virtual bool removeGlyph(const std::string& glyphName) = 0;
    virtual bool removeGlyph(uint32_t unicode) = 0;
    virtual bool replaceGlyphImage(const std::string& glyphName, 
                                  const std::vector<uint8_t>& newImage) = 0;
    
    // Информация
    virtual std::vector<GlyphInfo> listGlyphs() const = 0;
    virtual GlyphInfo getGlyphInfo(const std::string& glyphName) const = 0;
    
    // Утилиты
    virtual std::string findGlyphName(uint32_t unicode) const = 0;
};

class FontFormatHandler {
public:
    virtual ~FontFormatHandler() = default;
    virtual bool canHandle(const std::string& filepath) = 0;
    virtual std::unique_ptr<Font> loadFont(const std::string& filepath) = 0;
    virtual FontFormat getFormat() const = 0;
};

// Исключения
class FontException : public std::runtime_error {
public:
    FontException(const std::string& message) : std::runtime_error(message) {}
};

class FontLoadException : public FontException {
public:
    FontLoadException(const std::string& filename, const std::string& reason)
        : FontException("Failed to load font '" + filename + "': " + reason) {}
};

class FontFormatException : public FontException {
public:
    FontFormatException(const std::string& format, const std::string& reason)
        : FontException("Unsupported font format '" + format + "': " + reason) {}
};

class FontSaveException : public FontException {
public:
    FontSaveException(const std::string& filename, const std::string& reason)
        : FontException("Failed to save font '" + filename + "': " + reason) {}
};

class GlyphNotFoundException : public FontException {
public:
    GlyphNotFoundException(const std::string& glyphName)
        : FontException("Glyph not found: " + glyphName) {}
};

// Добавляем в конец файла
class TTFRebuilder {
public:
    virtual ~TTFRebuilder() = default;
    virtual std::vector<uint8_t> rebuild() = 0;
};


} // namespace fontmaster
