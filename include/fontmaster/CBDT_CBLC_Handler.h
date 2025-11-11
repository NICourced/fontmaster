#ifndef CBDT_CBLC_HANDLER_H
#define CBDT_CBLC_HANDLER_H

#include "fontmaster/FontHandler.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include <memory>
#include <vector>

namespace fontmaster {

class CBDT_CBLC_Handler : public FontHandler {
public:
    CBDT_CBLC_Handler();
    virtual ~CBDT_CBLC_Handler() = default;
    
    bool canHandle(const std::vector<uint8_t>& data) const override;
    std::unique_ptr<Font> loadFont(const std::string& filepath) override;
    
private:
    std::vector<std::string> supportedFormats;
};

} // namespace fontmaster

#endif // CBDT_CBLC_HANDLER_H
