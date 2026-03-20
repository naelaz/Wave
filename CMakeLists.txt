cmake_minimum_required(VERSION 3.20)

project(Wave VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────────────────────
set(WAVE_SOURCES
    src/main.cpp
    src/app/App.cpp
    src/window/MainWindow.cpp
    src/core/Log.cpp
    src/platform/Win32Helpers.cpp
    src/audio/Engine.cpp
)

set(WAVE_HEADERS
    src/app/App.h
    src/window/MainWindow.h
    src/core/Log.h
    src/platform/Win32Helpers.h
    src/audio/Engine.h
)

# ── Executable ────────────────────────────────────────────────
# WIN32 flag = Windows desktop app (WinMain entry, no console)
add_executable(Wave WIN32 ${WAVE_SOURCES} ${WAVE_HEADERS})

target_include_directories(Wave PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Link Win32 + Direct2D libs ────────────────────────────────
target_link_libraries(Wave PRIVATE
    d2d1
    dwrite
    dwmapi
    uxtheme
)

# ── Compiler options ──────────────────────────────────────────
target_compile_definitions(Wave PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(Wave PRIVATE
        /W4         # High warning level
        /permissive- # Standards conformance
        /utf-8      # Source and execution charset
    )
endif()
