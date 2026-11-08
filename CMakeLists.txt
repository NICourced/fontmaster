cmake_minimum_required(VERSION 3.10)
project(fontmaster VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTMASTER_RENDER REQUIRED IMPORTED_TARGET
    harfbuzz>=13 harfbuzz-raster>=13 harfbuzz-subset>=11.1 libpng cairo librsvg-2.0>=2.46 libturbojpeg>=3.0 libtiff-4>=4.5 libxml-2.0 zlib)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

# Include directories
include_directories(include)

# Core library sources
set(CORE_SOURCES
    src/core/FontMaster.cpp
)

# Utils sources
set(UTILS_SOURCES
    src/utils/CFFParser.cpp
    src/utils/CMAPParser.cpp
    src/utils/MAXPParser.cpp
    src/utils/NAMEParser.cpp
    src/utils/POSTParser.cpp
    src/utils/TTFRebuilder.cpp
    src/utils/TTFUtils.cpp
    src/utils/ImageCodec.cpp
)

# Format handlers sources
set(FORMATS_SOURCES
    src/formats/CBDT_CBLC_Font.cpp
    src/formats/CBDT_CBLC_Handler.cpp
    src/formats/CBDT_CBLC_Parser.cpp
    src/formats/CBDT_CBLC_Rebuilder.cpp
    src/formats/CBDT_CBLC_Registration.cpp
    src/formats/COLR_CPAL_Handler.cpp
    src/formats/COLR_CPAL_Registration.cpp
    src/formats/SBIX_Handler.cpp
    src/formats/SVG_Handler.cpp
    src/formats/SVGTable.cpp
    src/formats/SVGOverlayFont.cpp
    src/formats/StandardFont.cpp
    src/formats/Noto_Breeze_Adwaita_Nerd_Handler.cpp
    src/formats/Noto_Breeze_Adwaita_Nerd_Font.cpp
    src/formats/Noto_Breeze_Adwaita_Nerd_Registration.cpp
)

# Combine all sources
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${UTILS_SOURCES}
    ${FORMATS_SOURCES}
)

# Create main library
add_library(fontmaster SHARED ${ALL_SOURCES})
add_library(fontmaster::fontmaster ALIAS fontmaster)
target_compile_features(fontmaster PUBLIC cxx_std_17)
target_link_libraries(fontmaster PRIVATE PkgConfig::FONTMASTER_RENDER)

# Include directories for the library
target_include_directories(fontmaster
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set library properties
set_target_properties(fontmaster PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "fontmaster"
)
