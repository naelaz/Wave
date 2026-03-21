/*
 * Wave Plugin SDK v1
 * ====================
 * Include this single header in your plugin DLL.
 * Compile as a standard Windows DLL (C or C++).
 *
 * Your DLL must export these three functions:
 *
 *   WAVE_EXPORT WavePluginInfo* wave_plugin_get_info(void);
 *   WAVE_EXPORT int             wave_plugin_init(const WaveHostAPI* api);
 *   WAVE_EXPORT void            wave_plugin_shutdown(void);
 *
 * Optionally export:
 *   WAVE_EXPORT void wave_plugin_on_event(WaveEventType event);
 *
 * The host calls them in order: get_info → init → (events) → shutdown.
 * Return 0 from init on success, non-zero on failure.
 *
 * Use the WaveHostAPI* received in init() to:
 *   - Control playback (play, pause, stop, seek, volume)
 *   - Query playback info (title, artist, position, etc.)
 *   - Register commands (appear in Plugins menu)
 *   - Subscribe to events
 *   - Log messages
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Export macro ─────────────────────────────────────────── */

#ifdef WAVE_PLUGIN_BUILDING
  #define WAVE_EXPORT __declspec(dllexport)
#else
  #define WAVE_EXPORT __declspec(dllimport)
#endif

/* ── Version ─────────────────────────────────────────────── */

#define WAVE_PLUGIN_API_VERSION 1

/* ── Plugin metadata ─────────────────────────────────────── */

typedef struct WavePluginInfo {
    int         apiVersion;     /* must be WAVE_PLUGIN_API_VERSION */
    const char* id;             /* e.g. "com.wave.sample" */
    const char* name;           /* display name */
    const char* author;
    const char* version;        /* e.g. "1.0.0" */
    const char* description;
} WavePluginInfo;

/* ── Lifecycle states ────────────────────────────────────── */

typedef enum WavePluginState {
    WAVE_PLUGIN_DISCOVERED = 0,
    WAVE_PLUGIN_LOADED     = 1,
    WAVE_PLUGIN_ACTIVE     = 2,
    WAVE_PLUGIN_ERROR      = 3,
    WAVE_PLUGIN_UNLOADED   = 4,
} WavePluginState;

/* ── Events ──────────────────────────────────────────────── */

typedef enum WaveEventType {
    WAVE_EVENT_TRACK_CHANGED       = 1,
    WAVE_EVENT_PLAYBACK_STATE      = 2,
    WAVE_EVENT_VOLUME_CHANGED      = 3,
    WAVE_EVENT_LIBRARY_UPDATED     = 4,
    WAVE_EVENT_PLAYLIST_CHANGED    = 5,
    WAVE_EVENT_THEME_CHANGED       = 6,
    WAVE_EVENT_LAYOUT_CHANGED      = 7,
} WaveEventType;

/* ── Playback info snapshot ──────────────────────────────── */

typedef struct WavePlaybackInfo {
    int    state;           /* 0=stopped, 1=playing, 2=paused */
    double position;
    double duration;
    double volume;
    const wchar_t* title;
    const wchar_t* artist;
    const wchar_t* album;
    const wchar_t* filePath;
} WavePlaybackInfo;

/* ── Host API table ──────────────────────────────────────── */
/* Received in wave_plugin_init(). Store the pointer for later use. */

typedef struct WaveHostAPI {
    void   (*play)(void);
    void   (*pause)(void);
    void   (*stop)(void);
    void   (*seekTo)(double seconds);
    void   (*setVolume)(double vol);
    void   (*getPlaybackInfo)(WavePlaybackInfo* out);
    int    (*registerCommand)(const char* id, const char* name,
                               void (*callback)(void* userData), void* userData);
    int    (*subscribe)(WaveEventType event,
                         void (*callback)(WaveEventType event, void* userData),
                         void* userData);
    void   (*log)(const char* level, const char* message);
} WaveHostAPI;

/* ── Required plugin exports ─────────────────────────────── */

/* These are declared here for reference. Your plugin defines them. */
/*
WAVE_EXPORT WavePluginInfo* wave_plugin_get_info(void);
WAVE_EXPORT int             wave_plugin_init(const WaveHostAPI* api);
WAVE_EXPORT void            wave_plugin_shutdown(void);
WAVE_EXPORT void            wave_plugin_on_event(WaveEventType event);
*/

#ifdef __cplusplus
}
#endif
