#include "fontmaster/FontMaster.h"
#include <cassert>
#include <iostream>

void testCBDParsing() {
    std::cout << "Testing CBDT/CBLC parsing..." << std::endl;
    
    try {
        auto font = fontmaster::Font::load("test_emoji.ttf");
        assert(font != nullptr);
        assert(font->getFormat() == fontmaster::FontFormat::CBDT_CBLC);
        
        auto glyphs = font->listGlyphs();
        assert(!glyphs.empty());
        
        std::cout << "✓ CBDT/CBLC parsing test passed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ CBDT/CBLC parsing test failed: " << e.what() << std::endl;
        throw;
    }
}

void testGlyphRemoval() {
    std::cout << "Testing glyph removal..." << std::endl;
    
    try {
        auto font = fontmaster::Font::load("test_emoji.ttf");
        auto originalGlyphs = font->listGlyphs();
        
        if (!originalGlyphs.empty()) {
            const std::string testGlyph = originalGlyphs[0].name;
            bool removed = font->removeGlyph(testGlyph);
            assert(removed);
            
            auto newGlyphs = font->listGlyphs();
            assert(newGlyphs.size() == originalGlyphs.size() - 1);
            
            // Проверяем, что глиф действительно удален
            bool found = false;
            for (const auto& glyph : newGlyphs) {
                if (glyph.name == testGlyph) {
                    found = true;
                    break;
                }
            }
            assert(!found);
        }
        
        std::cout << "✓ Glyph removal test passed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Glyph removal test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    try {
        testCBDParsing();
        testGlyphRemoval();
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (...) {
        return 1;
    }
}
