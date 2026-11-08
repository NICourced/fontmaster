#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <utility>

namespace fontmaster {

class Font;

// Render the current font state, including edits that have not been saved.
// The result is a transparent square PNG. Sizes outside 1..4096 throw.
std::vector<uint8_t> renderGlyph(const Font& font, const std::string& glyphName,
                               int size = 128);

class EmojiRenderer {
public:
    virtual ~EmojiRenderer() = default;

    // Renders a single glyph to PNG data
    virtual std::vector<uint8_t> renderGlyph(const std::string& fontPath,
                                           const std::string& glyphName,
                                           int size = 64) = 0;

    // Get a list of available glyphs with their Unicode values
    virtual std::vector<std::pair<std::string, uint32_t>> getGlyphs(const std::string& fontPath) = 0;
};

// Factory function to create the appropriate renderer
std::unique_ptr<EmojiRenderer> createEmojiRenderer();

} // namespace fontmaster
