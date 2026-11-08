#ifndef NOTO_BREEZE_ADWAITA_NERD_HANDLER_H
#define NOTO_BREEZE_ADWAITA_NERD_HANDLER_H

#include "fontmaster/FontMaster.h"
#include <memory>
#include <vector>
#include <string>

namespace fontmaster {

class Noto_Breeze_Adwaita_Nerd_Handler : public FontFormatHandler {
public:
    Noto_Breeze_Adwaita_Nerd_Handler() = default;
    virtual ~Noto_Breeze_Adwaita_Nerd_Handler() = default;

    bool canHandle(const std::string& filepath) override;

    std::unique_ptr<Font> loadFont(const std::string& filepath) override;

    FontFormat getFormat() const override {
        return FontFormat::NOTO_BREEZE_ADWAITA_NERD;
    }

private:
    bool isNotoFont(const std::string& filepath) const;
    bool isNotoEmojiFont(const std::string& filepath) const;
    bool isBreezeFont(const std::string& filepath) const;
    bool isAdwaitaFont(const std::string& filepath) const;
    bool isNerdFont(const std::string& filepath) const;
    std::string extractFontFamily(const std::string& filepath) const;
};

} // namespace fontmaster

#endif // NOTO_BREEZE_ADWAITA_NERD_HANDLER_H
