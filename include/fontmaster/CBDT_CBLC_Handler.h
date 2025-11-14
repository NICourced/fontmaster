#ifndef CBDT_CBLC_HANDLER_H
#define CBDT_CBLC_HANDLER_H

#include "fontmaster/FontMaster.h"
#include <memory>
#include <vector>

namespace fontmaster {

class CBDT_CBLC_Handler : public FontFormatHandler { // Исправлено: добавлен public
public:
    CBDT_CBLC_Handler() = default;
    virtual ~CBDT_CBLC_Handler() = default;
    
    bool canHandle(const std::string& filepath) override;
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) override;
    
    FontFormat getFormat() const override { 
        return FontFormat::CBDT_CBLC; 
    }

private:
    std::vector<std::string> supportedFormats;
};

} // namespace fontmaster

#endif // CBDT_CBLC_HANDLER_H
