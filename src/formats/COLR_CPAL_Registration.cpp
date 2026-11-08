#include "fontmaster/FontMaster.h"
#include "fontmaster/COLR_CPAL_Handler.h"
#include <memory>

namespace fontmaster {

static void registerCOLRHandlerInternal() {
    // Create and register the handler
    auto handler = std::make_unique<COLR_CPAL_Handler>();
    registerHandler(std::move(handler));
}

void registerCOLRHandler() {
    registerCOLRHandlerInternal();
}

} // namespace fontmaster
