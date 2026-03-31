cmake_minimum_required(VERSION 3.25)

project(OpenScreen
    VERSION 2.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# Qt6
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    OpenGL
    OpenGLWidgets
    Multimedia
    MultimediaWidgets
    LinguistTools
)

# ---------------------------------------------------------------------------
# FFmpeg (prefer pkg-config, fall back to find_library)
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)

set(FFMPEG_LIBS avcodec avformat avutil swscale swresample)
set(FFMPEG_LINK_LIBS "")

foreach(lib IN LISTS FFMPEG_LIBS)
    string(TOUPPER ${lib} LIB_UPPER)

    if(PkgConfig_FOUND)
        pkg_check_modules(FF_${LIB_UPPER} QUIET lib${lib})
    endif()

    if(FF_${LIB_UPPER}_FOUND)
        list(APPEND FFMPEG_LINK_LIBS ${FF_${LIB_UPPER}_LIBRARIES})
        include_directories(${FF_${LIB_UPPER}_INCLUDE_DIRS})
        link_directories(${FF_${LIB_UPPER}_LIBRARY_DIRS})
    else()
        find_library(FF_${LIB_UPPER}_LIB NAMES ${lib} lib${lib})
        if(FF_${LIB_UPPER}_LIB)
            list(APPEND FFMPEG_LINK_LIBS ${FF_${LIB_UPPER}_LIB})
        else()
            message(WARNING "FFmpeg library '${lib}' not found – link may fail.")
        endif()
    endif()
endforeach()

find_path(FFMPEG_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    PATH_SUFFIXES ffmpeg
)
if(FFMPEG_INCLUDE_DIR)
    include_directories(${FFMPEG_INCLUDE_DIR})
endif()

# ---------------------------------------------------------------------------
# Third-party dependencies (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "")
set(BUILD_GMOCK ON CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(nlohmann_json spdlog googletest)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE OPENSCREEN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE OPENSCREEN_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# Separate main.cpp from the core library so tests can link without
# a duplicate main() symbol.
list(FILTER OPENSCREEN_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

set(OPENSCREEN_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/openscreen.qrc
)

# ---------------------------------------------------------------------------
# Core library (for linking into tests without a second compilation)
# ---------------------------------------------------------------------------
add_library(openscreen_core STATIC
    ${OPENSCREEN_SOURCES}
    ${OPENSCREEN_HEADERS}
)

target_include_directories(openscreen_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(openscreen_core PUBLIC
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ${FFMPEG_LINK_LIBS}
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(OpenScreen
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${OPENSCREEN_RESOURCES}
)

target_link_libraries(OpenScreen PRIVATE openscreen_core)

# Platform-specific executable properties
set_target_properties(OpenScreen PROPERTIES
    WIN32_EXECUTABLE $<BOOL:$<PLATFORM_ID:Windows>>
    MACOSX_BUNDLE   $<BOOL:$<PLATFORM_ID:Darwin>>
)

# ---------------------------------------------------------------------------
# Qt deploy helper
# ---------------------------------------------------------------------------
qt_generate_deploy_app_script(
    TARGET OpenScreen
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
