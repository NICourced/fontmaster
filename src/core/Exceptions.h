#pragma once
#include <stdexcept>
#include <string>

namespace fontmaster {

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

class GlyphOperationException : public FontException {
public:
    GlyphOperationException(const std::string& operation, const std::string& reason)
        : FontException("Glyph operation '" + operation + "' failed: " + reason) {}
};

} // namespace fontmaster
