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
    src/core/EmojiRenderer.cpp
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

# CLI executable
add_executable(fontmaster_cli
    src/cli/main.cpp
)

target_link_libraries(fontmaster_cli fontmaster)

# Qt GUI executable (optional - requires Qt)
find_package(Qt5 COMPONENTS Core Widgets QUIET)
if(Qt5_FOUND)
    qt5_wrap_cpp(QT_HEADERS
        src/qt/MainWindow.h
    )

    add_executable(fontmaster_qt
        src/qt/main.cpp
        src/qt/MainWindow.cpp
        ${QT_HEADERS}
    )

    target_link_libraries(fontmaster_qt fontmaster Qt5::Core Qt5::Widgets)

    # Set Qt specific properties
    set_target_properties(fontmaster_qt PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
else()
    message(WARNING "Qt5 not found - skipping fontmaster_qt target")
endif()

# Tests use explicit fixture paths and also work with out-of-tree builds.
include(CTest)
set(FONTMASTER_TEST_FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fonts" CACHE PATH "Font test fixtures")
if(BUILD_TESTING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cbdt_cblc.cpp")
        add_executable(fontmaster_tests_cbdt tests/test_cbdt_cblc.cpp)
        target_link_libraries(fontmaster_tests_cbdt fontmaster)
        if(EXISTS "${FONTMASTER_TEST_FONT_DIR}/NotoColorEmoji.ttf")
            add_test(NAME CBDT_CBLCTests COMMAND fontmaster_tests_cbdt
                "${FONTMASTER_TEST_FONT_DIR}/NotoColorEmoji.ttf")
        endif()
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noto_emojis.cpp")
        add_executable(fontmaster_tests_noto tests/test_noto_emojis.cpp)
        target_link_libraries(fontmaster_tests_noto fontmaster)
    endif()
    add_executable(fontmaster_binary_tests tests/test_binary_fonts.cpp)
    target_link_libraries(fontmaster_binary_tests fontmaster)
    foreach(component rendering svg sbix colr_images standard mixed_fonts)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${component}.cpp")
            add_executable(fontmaster_test_${component} "tests/test_${component}.cpp")
            target_include_directories(fontmaster_test_${component} PRIVATE src)
            target_link_libraries(fontmaster_test_${component} fontmaster PkgConfig::FONTMASTER_RENDER)
            if(component STREQUAL "sbix" OR component STREQUAL "standard" OR
               EXISTS "${FONTMASTER_TEST_FONT_DIR}/NotoColorEmoji.ttf")
                add_test(NAME ColorFonts_${component} COMMAND fontmaster_test_${component}
                    "${FONTMASTER_TEST_FONT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
            endif()
        endif()
    endforeach()
    if(EXISTS "${FONTMASTER_TEST_FONT_DIR}/NotoColorEmoji.ttf")
        foreach(mode load edit colr-edit composites metadata malformed)
            add_test(NAME BinaryFonts_${mode} COMMAND fontmaster_binary_tests "${mode}"
                "${FONTMASTER_TEST_FONT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        endforeach()
        # Independent file validation is available when FontTools is installed.
        find_program(FONTMASTER_PYTHON_EXECUTABLE NAMES python3)
        if(FONTMASTER_PYTHON_EXECUTABLE)
            execute_process(COMMAND "${FONTMASTER_PYTHON_EXECUTABLE}" -c "import fontTools"
                RESULT_VARIABLE FONTMASTER_HAS_FONTTOOLS OUTPUT_QUIET ERROR_QUIET)
            if(FONTMASTER_HAS_FONTTOOLS EQUAL 0)
                add_test(NAME BinaryFonts_fonttools COMMAND "${FONTMASTER_PYTHON_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_binary_fonts.py"
                    "$<TARGET_FILE:fontmaster_cli>" "${FONTMASTER_TEST_FONT_DIR}"
                    "${CMAKE_CURRENT_BINARY_DIR}")
                set_tests_properties(BinaryFonts_fonttools PROPERTIES
                    DEPENDS "BinaryFonts_edit;BinaryFonts_colr-edit")
                add_test(NAME ColorFonts_fonttools COMMAND "${FONTMASTER_PYTHON_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_color_fonts.py"
                    "$<TARGET_FILE:fontmaster_cli>" "${FONTMASTER_TEST_FONT_DIR}"
                    "${CMAKE_CURRENT_BINARY_DIR}")
                set_tests_properties(ColorFonts_fonttools PROPERTIES
                    DEPENDS "ColorFonts_svg;ColorFonts_sbix;ColorFonts_colr_images;ColorFonts_standard;ColorFonts_mixed_fonts")
            endif()
        endif()
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_all_fonts.cpp")
        add_executable(test_all_fonts test_all_fonts.cpp)
        target_link_libraries(test_all_fonts fontmaster)
    endif()
endif()

# Installation supports system and user prefixes, --prefix and DESTDIR staging.
install(TARGETS fontmaster EXPORT fontmaster-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(fontmaster_app_targets fontmaster_cli)
if(TARGET fontmaster_qt)
    list(APPEND fontmaster_app_targets fontmaster_qt)
endif()

# Find the installed library without ldconfig or LD_LIBRARY_PATH, including
# after moving an installation or overriding its prefix at install time.
if(UNIX)
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(fontmaster_install_rpath "${CMAKE_INSTALL_LIBDIR}")
    else()
        file(RELATIVE_PATH fontmaster_bin_to_lib
            "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
        if(APPLE)
            set(fontmaster_install_rpath "@loader_path/${fontmaster_bin_to_lib}")
        else()
            set(fontmaster_install_rpath "$ORIGIN/${fontmaster_bin_to_lib}")
        endif()
    endif()
    set_property(TARGET ${fontmaster_app_targets} APPEND PROPERTY
        INSTALL_RPATH "${fontmaster_install_rpath}")
endif()

install(TARGETS ${fontmaster_app_targets}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/fontmaster DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(fontmaster_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/fontmaster")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fontmaster-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config.cmake"
    INSTALL_DESTINATION "${fontmaster_cmake_dir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config-version.cmake"
    DESTINATION "${fontmaster_cmake_dir}"
)
install(EXPORT fontmaster-targets
    FILE fontmaster-targets.cmake
    NAMESPACE fontmaster::
    DESTINATION "${fontmaster_cmake_dir}"
)
