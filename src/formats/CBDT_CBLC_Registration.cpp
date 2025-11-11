#include "fontmaster/CBDT_CBLC_Handler.h"
#include "fontmaster/FontMasterImpl.h"

namespace fontmaster {

bool registerCBDTHandler() {
    FontMaster::instance().registerHandler(
        std::make_unique<CBDT_CBLC_Handler>()
    );
    return true;
}

// Автоматическая регистрация при загрузке библиотеки
static bool registered = registerCBDTHandler();

} // namespace fontmaster
