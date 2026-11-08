#include "fontmaster/FontMaster.h"
#include "fontmaster/CBDT_CBLC_Handler.h"
#include <memory>

namespace fontmaster {

static void registerCBDTHandlerInternal() {
    auto handler = std::make_unique<CBDT_CBLC_Handler>();
    registerHandler(std::move(handler));
}

void registerCBDTHandler() {
    registerCBDTHandlerInternal();
}

} // namespace fontmaster
