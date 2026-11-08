#pragma once
#include "fontmaster/FontMaster.h"
#include <string>
#include <vector>
#include <memory>

namespace fontmaster {

class COLR_CPAL_Handler : public FontFormatHandler {
public:
    bool canHandle(const std::string& filepath) override;
    std::unique_ptr<Font> loadFont(const std::string& filepath) override;
    FontFormat getFormat() const override;
};

} // namespace fontmaster
