#pragma once
#include "fontmaster/FontMaster.h"
#include <set>
namespace fontmaster::detail {
std::unique_ptr<Font> createStandardFont(const std::string &path, FontFormat format = FontFormat::STANDARD);
std::set<uint16_t> outlineGlyphs(const std::vector<uint8_t> &data);
// Remove the outline and Unicode mappings, retaining every other glyph ID.
// Throws when another glyph depends on this outline or the subset is unsafe.
std::vector<uint8_t> removeOutlineGlyph(const std::vector<uint8_t> &data, uint16_t gid);
} // namespace fontmaster::detail
