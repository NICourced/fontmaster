#include "fontmaster/FontMaster.h"
#include "fontmaster/TTFUtils.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <fstream>
#include <map>
#include <iostream>

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
        std::cout << "Registered handler for format: " << static_cast<int>(format) << std::endl;
    }
    
    std::unique_ptr<Font> loadFont(const std::string& filepath) {
        // Сначала проверяем существование файла
        std::ifstream testFile(filepath, std::ios::binary);
        if (!testFile) {
            throw FontLoadException(filepath, "File not found or cannot be opened");
        }
        testFile.close();
        
        // Читаем данные файла для анализа
        std::ifstream file(filepath, std::ios::binary);
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (size == 0) {
            throw FontLoadException(filepath, "File is empty");
        }
        
        std::vector<uint8_t> headerData(std::min(size, size_t(1024)), 0);
        file.read(reinterpret_cast<char*>(headerData.data()), headerData.size());
        file.close();
        
        // Пробуем все обработчики по порядку
        for (auto& handler : handlers) {
            try {
                if (handler->canHandle(filepath)) {
                    std::cout << "Loading font with handler: " << static_cast<int>(handler->getFormat()) << std::endl;
                    return handler->loadFont(filepath);
                }
            } catch (const std::exception& e) {
                std::cerr << "Handler error: " << e.what() << std::endl;
                continue;
            }
        }
        
        throw FontLoadException(filepath, "No suitable handler found for this font format");
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

static void registerAllHandlers() {
    // Регистрируем обработчики
    registerCBDTHandler();
    registerSBIXHandler();
    registerCOLRHandler();
    registerSVGHandler();
}

// Вызываем регистрацию при загрузке библиотеки
__attribute__((constructor))
static void initFontMaster() {
    std::cout << "Initializing FontMaster..." << std::endl;
    registerAllHandlers();
}

std::unique_ptr<Font> Font::load(const std::string& filepath) {
    return FontMasterImpl::instance().loadFont(filepath);
}

} // namespace fontmaster
