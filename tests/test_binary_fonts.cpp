#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Font.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/CMAPParser.h"
#include "fontmaster/POSTParser.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace fontmaster;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Expected mode, font directory and output directory");
        const std::string mode = argv[1];
        const std::filesystem::path fonts = argv[2], output = argv[3];
        if (mode == "load") {
            const std::pair<const char*, size_t> cases[] = {
                {"NotoColorEmoji.ttf", 3985},
                {"NotoColorEmoji-flagsonly.ttf", 278},
                {"NotoColorEmoji-noflags.ttf", 3718},
                {"NotoColorEmoji-emojicompat.ttf", 3985},
                {"NotoColorEmoji_WindowsCompatible.ttf", 3985},
                {"Noto-COLRv1.ttf", 3993},
                {"Noto-COLRv1-noflags.ttf", 3708},
                {"Noto-COLRv1-emojicompat.ttf", 3993}
            };
            for (const auto& entry : cases) {
                auto font = Font::load((fonts / entry.first).string());
                require(font != nullptr, std::string("Cannot load ") + entry.first);
                auto glyphs = font->listGlyphs();
                require(glyphs.size() == entry.second, std::string(entry.first) + ": incorrect glyph count " + std::to_string(glyphs.size()));
                for (const auto& glyph : glyphs) {
                    if (font->getFormat() == FontFormat::CBDT_CBLC) {
                        require(glyph.format == "png", "Incorrect PNG format");
                        require(glyph.image_data.size() > 8 && glyph.image_data[0] == 0x89, "Missing PNG payload");
                        require(glyph.image_data.size() == glyph.data_size, "Incorrect payload length");
                    }
                }
                require(font->getGlyphInfo(glyphs.front().name).name == glyphs.front().name, "Glyph lookup failed");
            }
        } else if (mode == "edit") {
            auto font = Font::load((fonts / "NotoColorEmoji.ttf").string());
            auto original = font->listGlyphs();
            require(original.size() > 2, "No glyphs available for editing");
            const auto removedName = font->findGlyphName(0x1F600);
            const auto replacementName = font->findGlyphName(0x1F601);
            require(!removedName.empty() && !replacementName.empty(), "Unicode lookup failed");
            const auto replacement = font->getGlyphInfo(replacementName).image_data;
            require(!font->replaceGlyphImage(replacementName, {1, 2, 3}), "Invalid image was accepted");
            auto corrupt = replacement;
            corrupt.at(corrupt.size() / 2) ^= 1;
            require(!font->replaceGlyphImage(replacementName, corrupt), "Corrupted PNG pixels or CRC accepted");
            require(font->removeGlyph(0x1F600), "Cannot remove glyph");
            require(!font->removeGlyph(0x1F600), "Removed glyph can be removed again");
            require(font->listGlyphs().size() == original.size() - 1, "Removal did not update the glyph list");
            require(font->findGlyphName(0x1F600).empty(), "Removed glyph remains visible by Unicode");
            const auto target = font->findGlyphName(0x1F602);
            require(font->replaceGlyphImage(target, replacement), "Cannot replace PNG");
            require(font->getGlyphInfo(target).image_data == replacement, "Replacement did not update data");
            const auto saved = (output / "edited.ttf").string();
            require(font->save(saved), "Cannot save edited font");
            auto reloaded = Font::load(saved);
            require(reloaded != nullptr, "Cannot reload edited font");
            require(reloaded->listGlyphs().size() == original.size() - 1, "Removal was not persisted");
            require(reloaded->getGlyphInfo(target).image_data == replacement, "Replacement was not persisted");
            require(reloaded->findGlyphName(0x1F600).empty(), "Removal lost after reload");
            require(reloaded->save((output / "edited-again.ttf").string()), "Cannot save twice");
            auto* bitmap = dynamic_cast<CBDT_CBLC_Font*>(font.get());
            require(bitmap != nullptr, "Wrong bitmap handler");
            bitmap->setFontData(reloaded->getFontData());
            require(bitmap->listGlyphs().size() == original.size() - 1, "setFontData kept stale parser state");
        } else if (mode == "colr-edit") {
            auto font = Font::load((fonts / "Noto-COLRv1.ttf").string());
            const auto count = font->listGlyphs().size();
            require(!font->removeGlyph("glyph_65549"), "Out-of-range glyph ID wrapped to another glyph");
            require(!font->removeGlyph("glyph_13junk"), "Trailing garbage in glyph ID accepted");
            const auto name = font->findGlyphName(0x1F600);
            require(!name.empty(), "COLRv1 Unicode lookup failed");
            require(!font->replaceGlyphImage(name, {1,2,3}), "COLR accepted a fake image replacement");
            require(font->removeGlyph(name), "COLRv1 removal failed");
            require(font->listGlyphs().size() == count - 1, "COLRv1 removal did not update list");
            require(!font->removeGlyph(name), "COLRv1 removed glyph twice");
            const auto path = (output / "edited-colr.ttf").string();
            require(font->save(path), "Cannot save COLRv1");
            auto reloaded = Font::load(path);
            require(reloaded->listGlyphs().size() == count - 1, "COLRv1 removal was not persisted");
            require(reloaded->findGlyphName(0x1F600).empty(), "COLRv1 removed glyph still found");
        } else if (mode == "composites") {
            auto loaded = Font::load((fonts / "NotoColorEmoji.ttf").string());
            auto* bitmap = dynamic_cast<CBDT_CBLC_Font*>(loaded.get());
            auto strikes = bitmap->getStrikes();
            auto& images = strikes.begin()->second.glyphImages;
            const auto component = images.begin()->first;
            auto composite = std::next(images.begin());
            const auto compositeID = composite->first;
            auto& image = composite->second;
            image.imageFormat = 8;
            image.recordData.resize(5);
            image.recordData.insert(image.recordData.end(), {0, 0,1,
                static_cast<uint8_t>(component >> 8), static_cast<uint8_t>(component), 0,0});
            CBDT_CBLC_Rebuilder rebuilder(loaded->getFontData(), strikes, {});
            CBDT_CBLC_Font font("");
            font.setFontData(rebuilder.rebuild());
            require(font.load(), "Cannot read bitmap composite fixture");
            require(!font.removeGlyph("glyph_" + std::to_string(component)), "Removed a component of a live bitmap composite");
            require(font.removeGlyph("glyph_" + std::to_string(compositeID)), "Cannot remove bitmap composite");
            require(font.removeGlyph("glyph_" + std::to_string(component)), "Unused bitmap component is still protected");
        } else if (mode == "metadata") {
            std::vector<uint8_t> post(32, 0);
            post[1] = 2;
            post.insert(post.end(), {0,3, 0,0, 1,2, 1,3, 3,'f','o','o', 6,'b','a','r','B','a','z'});
            fontmaster::utils::POSTParser names(post, 0, 3);
            require(names.parse(), "Cannot parse POST version 2");
            const auto& entries = names.getGlyphNames();
            require(entries.at(1) == "foo" && entries.at(2) == "barBaz", "POST custom name indices are not decoded correctly");
        } else if (mode == "malformed") {
            std::ifstream input(fonts / "NotoColorEmoji.ttf", std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
            require(!bytes.empty(), "Missing fixture");
            bytes.resize(100);
            CBDT_CBLC_Parser parser(bytes);
            require(!parser.parse(), "Truncated font accepted");
            bytes.assign(12, 0);
            CBDT_CBLC_Parser invalid(bytes);
            require(!invalid.parse(), "Invalid signature accepted");
            std::vector<uint8_t> cmap{0,0,0,1, 0,3,0,1, 0,0,0,12,
                                      0,4,0,16, 0,0,0,2, 0,0,0,0,0,0,0,0};
            fontmaster::utils::CMAPParser truncatedCmap(cmap);
            require(!truncatedCmap.parse(), "Truncated cmap segment arrays accepted");
        } else {
            throw std::runtime_error("Unknown test mode");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
