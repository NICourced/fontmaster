#pragma once
#include "fontmaster/FontMaster.h"
#include <memory>

namespace fontmaster::detail {
// Keep native SVG presentations visible when a bitmap handler owns the font.
std::unique_ptr<Font> withSVGPresentations(std::unique_ptr<Font> primary);
} // namespace fontmaster::detail
