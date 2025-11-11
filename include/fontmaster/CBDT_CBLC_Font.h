#ifndef CBDT_CBLC_FONT_H
#define CBDT_CBLC_FONT_H

#include "fontmaster/Font.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include <string>
#include <vector>

namespace fontmaster {

class CBDT_CBLC_Font : public Font {
public:
    CBDT_CBLC_Font(const std::string& filepath);
    virtual ~CBDT_CBLC_Font() = default;
    
    bool load() override;
    bool save(const std::string& filepath) override;
    
    const std::vector<uint8_t>& getFontData() const override;
    void setFontData(const std::vector<uint8_t>& data) override;
    
    // CBDT/CBLC specific methods
    const std::map<uint32_t, StrikeRecord>& getStrikes() const { return parser.getStrikes(); }
    const std::vector<uint16_t>& getRemovedGlyphs() const { return parser.getRemovedGlyphs(); }
    
private:
    std::string filepath;
    std::vector<uint8_t> fontData;
    CBDT_CBLC_Parser parser;
};

} // namespace fontmaster

#endif // CBDT_CBLC_FONT_H
