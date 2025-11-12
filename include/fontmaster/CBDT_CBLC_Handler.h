#ifndef CBDT_CBLC_HANDLER_H
#define CBDT_CBLC_HANDLER_H

#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Parser.h"
#include "fontmaster/CBDT_CBLC_Rebuilder.h"
#include <memory>
#include <vector>

namespace fontmaster {

class CBDT_CBLC_Handler : FontFormatHandler {
public:
    CBDT_CBLC_Handler();
    virtual ~CBDT_CBLC_Handler() = default;
    
    bool canHandle(const std::string& filepath) override; 
    bool canHandle(const std::vector<unsigned char>& filePath) const;
    
    std::unique_ptr<Font> loadFont(const std::string& filepath);
    
private:
    std::vector<std::string> supportedFormats;
};

} // namespace fontmaster

#endif // CBDT_CBLC_HANDLER_H
