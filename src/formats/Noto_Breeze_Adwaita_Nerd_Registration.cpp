#include "fontmaster/FontMaster.h"
#include "fontmaster/Noto_Breeze_Adwaita_Nerd_Handler.h"
#include <memory>

namespace fontmaster {

static void registerNotoBreezeAdwaitaNerdHandlerInternal() {
    // Create and register the handler
    auto handler = std::make_unique<Noto_Breeze_Adwaita_Nerd_Handler>();
    registerHandler(std::move(handler));
}

void registerNotoBreezeAdwaitaNerdHandler() {
    registerNotoBreezeAdwaitaNerdHandlerInternal();
}

} // namespace fontmaster
