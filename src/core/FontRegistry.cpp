#include "fontmaster/FontMaster.h"
#include <algorithm>
#include <memory>

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
        // Пробуем все обработчики по порядку
        for (auto& handler : handlers) {
            if (handler->canHandle(filepath)) {
                std::cout << "Loading font with handler: " << static_cast<int>(handler->getFormat()) << std::endl;
                return handler->loadFont(filepath);
            }
        }
        
        std::cerr << "No suitable handler found for font: " << filepath << std::endl;
        return nullptr;
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
static void registerAllHandlers() {
    auto& instance = FontMasterImpl::instance();
    
    // Обработчики регистрируются автоматически через static переменные
    // в своих .cpp файлах
}

// Вызываем регистрацию при загрузке библиотеки
__attribute__((constructor))
static void initFontMaster() {
    registerAllHandlers();
}

} // namespace fontmaster
