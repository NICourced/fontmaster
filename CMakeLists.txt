cmake_minimum_required(VERSION 3.10)
project(fontmaster VERSION 1.0.0 LANGUAGES CXX)

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
)

# Format handlers sources
set(FORMATS_SOURCES
    src/formats/CBDT_CBLC_Font.cpp
    src/formats/CBDT_CBLC_Handler.cpp
    src/formats/CBDT_CBLC_Parser.cpp
    src/formats/CBDT_CBLC_Rebuilder.cpp
    #    src/formats/CBDT_CBLC_Registration.cpp
    src/formats/COLR_CPAL_Handler.cpp
    src/formats/SBIX_Handler.cpp
    src/formats/SVG_Handler.cpp
)

# Combine all sources
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${UTILS_SOURCES}
    ${FORMATS_SOURCES}
)

# Create main library
add_library(fontmaster SHARED ${ALL_SOURCES})

# Include directories for the library
target_include_directories(fontmaster 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
    src/cli/CommandProcessor.cpp
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

# Tests (optional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cbdt_cblc.cpp")
    enable_testing()
    
    add_executable(fontmaster_tests
        tests/test_cbdt_cblc.cpp
    )
    
    target_link_libraries(fontmaster_tests fontmaster)
    
    add_test(NAME CBDT_CBLCTests COMMAND fontmaster_tests)
endif()

# Installation
install(TARGETS fontmaster fontmaster_cli
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

if(TARGET fontmaster_qt)
    install(TARGETS fontmaster_qt
        RUNTIME DESTINATION bin
    )
endif()

install(DIRECTORY include/fontmaster
    DESTINATION include
)

# Package configuration (optional)
#include(CMakePackageConfigHelpers)

#write_basic_package_version_file(
#    "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config-version.cmake"
#    VERSION ${PROJECT_VERSION}
#    COMPATIBILITY AnyNewerVersion
#)

#configure_package_config_file(
#    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fontmaster-config.cmake.in
#    ${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config.cmake
#    INSTALL_DESTINATION lib/cmake/fontmaster
#)

#install(FILES
#    ${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config.cmake
#    ${CMAKE_CURRENT_BINARY_DIR}/fontmaster-config-version.cmake
#    DESTINATION lib/cmake/fontmaster
#)

# Export targets
#export(EXPORT fontmaster-targets
#    FILE "${CMAKE_CURRENT_BINARY_DIR}/fontmaster-targets.cmake"
#)

#install(EXPORT fontmaster-targets
#    FILE fontmaster-targets.cmake
#    DESTINATION lib/cmake/fontmaster
#)
