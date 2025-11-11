#ifndef CBDT_CBLC_FONT_H
#define CBDT_CBLC_FONT_H

#include "fontmaster/Font.h"
#include <string>
#include <vector>
#include <map>

namespace fontmaster {

class CBDT_CBLC_Font : public Font {
public:
    explicit CBDT_CBLC_Font(const std::string& filepath);
    
    bool load() override;
    bool save(const std::string& filepath) override;
    
    const std::vector<uint8_t>& getFontData() const override;
    void setFontData(const std::vector<uint8_t>& data) override;

private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    std::map<uint32_t, StrikeRecord> glyphStrikes;
    std::vector<uint16_t> removedGlyphs;
};

} // namespace fontmaster

#endif // CBDT_CBLC_FONT_H
