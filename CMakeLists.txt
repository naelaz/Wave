cmake_minimum_required(VERSION 3.20)

project(Wave VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Version defines ──────────────────────────────────────────
set(WAVE_VERSION_MAJOR 0)
set(WAVE_VERSION_MINOR 1)
set(WAVE_VERSION_PATCH 0)
set(WAVE_VERSION_STR "${WAVE_VERSION_MAJOR}.${WAVE_VERSION_MINOR}.${WAVE_VERSION_PATCH}-beta")

# ── Source files ──────────────────────────────────────────────
set(WAVE_SOURCES
    src/main.cpp
    src/app/App.cpp
    src/window/MainWindow.cpp
    src/core/Log.cpp
    src/platform/Win32Helpers.cpp
    src/audio/Engine.cpp
    src/audio/AudioCapture.cpp
    src/audio/WaveformCache.cpp
    src/ui/Renderer.cpp
    src/library/Library.cpp
    src/metadata/Metadata.cpp
    src/playlist/PlaylistManager.cpp
    src/settings/Settings.cpp
    src/layout/Layout.cpp
    src/theme/Theme.cpp
    src/visualizer/Visualizer.cpp
    src/plugin/PluginHost.cpp
    src/art/AlbumArt.cpp
    src/coverflow/CoverFlow.cpp
    src/audio/Equalizer.cpp
)

set(WAVE_HEADERS
    src/app/App.h
    src/window/MainWindow.h
    src/core/Log.h
    src/platform/Win32Helpers.h
    src/audio/Engine.h
    src/audio/AudioCapture.h
    src/audio/AudioSettings.h
    src/audio/WaveformCache.h
    src/ui/Renderer.h
    src/library/Library.h
    src/metadata/Metadata.h
    src/playlist/PlaylistManager.h
    src/settings/Settings.h
    src/layout/Layout.h
    src/theme/Theme.h
    src/visualizer/Visualizer.h
    src/plugin/PluginAPI.h
    src/plugin/PluginTypes.h
    src/plugin/PluginHost.h
    src/art/AlbumArt.h
    src/coverflow/CoverFlow.h
    src/audio/Equalizer.h
)

# ── Resources ─────────────────────────────────────────────────
set(WAVE_RESOURCES
    src/resources/resources.rc
)

# ── Executable ────────────────────────────────────────────────
add_executable(Wave WIN32 ${WAVE_SOURCES} ${WAVE_HEADERS} ${WAVE_RESOURCES})

target_include_directories(Wave PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Version defines passed to code ────────────────────────────
target_compile_definitions(Wave PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    WAVE_VERSION_MAJOR=${WAVE_VERSION_MAJOR}
    WAVE_VERSION_MINOR=${WAVE_VERSION_MINOR}
    WAVE_VERSION_PATCH=${WAVE_VERSION_PATCH}
    WAVE_VERSION_STR="${WAVE_VERSION_STR}"
)

# ── Link Win32 + Direct2D libs ────────────────────────────────
target_link_libraries(Wave PRIVATE
    d2d1
    dwrite
    dwmapi
    uxtheme
    comdlg32
    ole32
)

# ── Compiler options ──────────────────────────────────────────
if(MSVC)
    target_compile_options(Wave PRIVATE
        /W4
        /permissive-
        /utf-8
    )
    # Release: full optimization, no debug runtime, small binary
    target_compile_options(Wave PRIVATE
        $<$<CONFIG:Release>:/O2 /GL /GS->
    )
    target_link_options(Wave PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()

# ── Post-build: copy libmpv DLL next to exe ───────────────────
# Searches common locations for the DLL
set(MPV_DLL_NAMES "libmpv-2.dll" "mpv-2.dll")
foreach(CFG Debug Release)
    foreach(DLLNAME ${MPV_DLL_NAMES})
        set(MPV_SRC "${CMAKE_BINARY_DIR}/${CFG}/${DLLNAME}")
        if(EXISTS "${MPV_SRC}")
            # Already there, nothing to do
        endif()
    endforeach()
endforeach()

# ── Create plugins directory ──────────────────────────────────
add_custom_command(TARGET Wave POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Wave>/plugins"
    COMMENT "Ensuring plugins directory exists"
)

# ── Sample Plugin DLL ────────────────────────────────────────
add_library(wave_sample_plugin SHARED plugins/sample/sample_plugin.c)
target_compile_definitions(wave_sample_plugin PRIVATE WAVE_PLUGIN_BUILDING)
target_include_directories(wave_sample_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

set_target_properties(wave_sample_plugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Debug/plugins"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/plugins"
    OUTPUT_NAME "wave_sample_plugin"
)
