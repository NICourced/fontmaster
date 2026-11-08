#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include "fontmaster/Noto_Breeze_Adwaita_Nerd_Handler.h"
#include "formats/StandardFont.h"
#include "formats/SVGOverlayFont.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <fstream>
#include <map>
#include <iostream>
#include <mutex>

namespace fontmaster {

class FontMasterImpl {
private:
    std::vector<std::unique_ptr<FontFormatHandler>> handlers;
    std::map<FontFormat, FontFormatHandler*> handlerMap;

public:
    static FontMasterImpl& instance() {
        static FontMasterImpl instance;
        return instance;
    }

    void registerHandler(std::unique_ptr<FontFormatHandler> handler) {
        FontFormat format = handler->getFormat();
        handlers.push_back(std::move(handler));
        handlerMap[format] = handlers.back().get();
    }

    std::unique_ptr<Font> loadFont(const std::string& filepath) {
        try {
            const auto data = utils::readFontFile(filepath);
            if (data.empty()) throw std::runtime_error("File is empty");
            utils::parseTTFTables(data);
        } catch (const std::exception& error) {
            throw FontLoadException(filepath, error.what());
        }

        // Пробуем все обработчики по порядку
        for (auto& handler : handlers) {
            if (!handler->canHandle(filepath)) continue;
            // A recognized but damaged font must not fall through to a less
            // specific handler and masquerade as a successfully loaded font.
            try {
                auto font = handler->loadFont(filepath);
                if (!font) throw std::runtime_error("Format handler failed to parse font");
                return detail::withSVGPresentations(std::move(font));
            } catch (const std::exception& error) {
                throw FontLoadException(filepath, error.what());
            }
        }

        try { return detail::createStandardFont(filepath); }
        catch (const std::exception& error) { throw FontLoadException(filepath, error.what()); }
    }

    FontFormat detectFormat(const std::string& filepath) {
        for (auto& handler : handlers) {
            if (handler->canHandle(filepath)) {
                return handler->getFormat();
            }
        }
        return FontFormat::UNKNOWN;
    }

    std::vector<FontFormat> getSupportedFormats() {
        std::vector<FontFormat> formats;
        for (auto& handler : handlers) {
            formats.push_back(handler->getFormat());
        }
        return formats;
    }
};

// Глобальная регистрация обработчиков
extern void registerCBDTHandler();
extern void registerSBIXHandler();
extern void registerCOLRHandler();
extern void registerSVGHandler();
extern void registerNotoBreezeAdwaitaNerdHandler();

static void registerAllHandlers() {
    // Регистрируем обработчики
    registerCBDTHandler();
    registerSBIXHandler();
    registerCOLRHandler();
    registerSVGHandler();
    registerNotoBreezeAdwaitaNerdHandler();
}

std::unique_ptr<Font> Font::load(const std::string& filepath) {
    static std::once_flag initialized;
    std::call_once(initialized, registerAllHandlers);
    return FontMasterImpl::instance().loadFont(filepath);
}

void registerHandler(std::unique_ptr<FontFormatHandler> handler) {
    FontMasterImpl::instance().registerHandler(std::move(handler));
}

} // namespace fontmaster
