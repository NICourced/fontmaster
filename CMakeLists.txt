cmake_minimum_required(VERSION 3.16)
project(fontmaster VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройки компиляции
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Все бинарники и библиотеки в папку build/
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(FONTMASTER_DEBUG)
    add_compile_options(-g -O0 -DDEBUG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

# Предупреждения
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-maybe-uninitialized)
    endif()
endif()

# Включение всех исходных файлов
set(SOURCES
    src/core/FontMaster.cpp
    src/utils/TTFUtils.cpp
    src/utils/TTFRebuilder.cpp
    src/utils/CMAPParser.cpp
    src/utils/NAMEParser.cpp
    src/utils/POSTParser.cpp
    src/utils/MAXPParser.cpp
    src/utils/CFFParser.cpp
    src/formats/CBDT_CBLC_Parser.cpp
    src/formats/CBDT_CBLC_Handler.cpp
    src/formats/CBDT_CBLC_Rebuilder.cpp
    src/formats/SBIX_Handler.cpp
    src/formats/COLR_CPAL_Handler.cpp
    src/formats/SVG_Handler.cpp
)

set(HEADERS
    include/fontmaster/FontMaster.h
    include/fontmaster/TTFUtils.h
    include/fontmaster/TTFRebuilder.h
    include/fontmaster/CMAPParser.h
    include/fontmaster/NAMEParser.h
    include/fontmaster/POSTParser.h
    include/fontmaster/MAXPParser.h
    include/fontmaster/CFFParser.h
    src/formats/CBDT_CBLC_Rebuilder.h
)

# Основная библиотека
add_library(fontmaster SHARED ${SOURCES} ${HEADERS})

target_include_directories(fontmaster 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        src/core
        src/utils
        src/formats
)

target_compile_definitions(fontmaster PRIVATE FONTMASTER_LIBRARY)

# CLI приложение
add_executable(fontmaster-cli
    src/cli/main.cpp
)

target_link_libraries(fontmaster-cli fontmaster)

# Поиск Qt6
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

# Qt приложение
add_executable(fontmaster-qt
    src/qt/main.cpp
    src/qt/MainWindow.cpp
)

target_link_libraries(fontmaster-qt 
    Qt6::Core 
    Qt6::Widgets 
    Qt6::Gui 
    fontmaster
)

# Отключаем автоматическую установку
# install(TARGETS ...) - закомментировано

# Информация о сборке
message(STATUS "FontMaster build configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}")

# Дополнительная цель для копирования необходимых файлов
add_custom_target(copy-resources ALL
    COMMENT "Copying resources to build directory"
)

# Сообщение о завершении сборки
add_custom_command(TARGET copy-resources POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build completed successfully!"
    COMMAND ${CMAKE_COMMAND} -E echo "Binaries are in: ${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  CLI: ./fontmaster-cli --help"
    COMMAND ${CMAKE_COMMAND} -E echo "  GUI: ./fontmaster-qt"
)
