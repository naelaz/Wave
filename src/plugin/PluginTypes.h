#pragma once

#include <cstdint>

// ── Wave Plugin ABI ──────────────────────────────────────────
// Pure C types for DLL boundary. No std::, no templates, no exceptions.
// Plugins export C functions and the host calls them through this ABI.

#ifdef __cplusplus
extern "C" {
#endif

// ── Plugin metadata ──────────────────────────────────────────

#define WAVE_PLUGIN_API_VERSION 1

typedef struct WavePluginInfo {
    int         apiVersion;     // must be WAVE_PLUGIN_API_VERSION
    const char* id;             // unique ID, e.g. "com.example.myplugin"
    const char* name;           // display name
    const char* author;
    const char* version;        // e.g. "1.0.0"
    const char* description;
} WavePluginInfo;

// ── Plugin lifecycle ─────────────────────────────────────────

typedef enum WavePluginState {
    WAVE_PLUGIN_DISCOVERED = 0,
    WAVE_PLUGIN_LOADED     = 1,
    WAVE_PLUGIN_ACTIVE     = 2,
    WAVE_PLUGIN_ERROR      = 3,
    WAVE_PLUGIN_UNLOADED   = 4,
} WavePluginState;

// ── Events ───────────────────────────────────────────────────

typedef enum WaveEventType {
    WAVE_EVENT_TRACK_CHANGED       = 1,
    WAVE_EVENT_PLAYBACK_STATE      = 2,
    WAVE_EVENT_VOLUME_CHANGED      = 3,
    WAVE_EVENT_LIBRARY_UPDATED     = 4,
    WAVE_EVENT_PLAYLIST_CHANGED    = 5,
    WAVE_EVENT_THEME_CHANGED       = 6,
    WAVE_EVENT_LAYOUT_CHANGED      = 7,
} WaveEventType;

// ── Playback info snapshot ───────────────────────────────────

typedef struct WavePlaybackInfo {
    int    state;       // 0=stopped, 1=playing, 2=paused
    double position;
    double duration;
    double volume;
    const wchar_t* title;
    const wchar_t* artist;
    const wchar_t* album;
    const wchar_t* filePath;
} WavePlaybackInfo;

// ── Host API table passed to plugins ─────────────────────────
// Plugins receive this struct in wave_plugin_init().
// All function pointers are provided by the host.

typedef struct WaveHostAPI {
    // Playback control
    void   (*play)(void);
    void   (*pause)(void);
    void   (*stop)(void);
    void   (*seekTo)(double seconds);
    void   (*setVolume)(double vol);

    // Playback query — caller must NOT free returned strings
    void   (*getPlaybackInfo)(WavePlaybackInfo* out);

    // Command registration
    // id: unique command id, name: display name, callback: called when invoked
    int    (*registerCommand)(const char* id, const char* name,
                               void (*callback)(void* userData), void* userData);

    // Event subscription
    // callback called when event fires
    int    (*subscribe)(WaveEventType event,
                         void (*callback)(WaveEventType event, void* userData),
                         void* userData);

    // Logging
    void   (*log)(const char* level, const char* message);
} WaveHostAPI;

// ── Functions a plugin DLL must export ───────────────────────

// Required: return plugin metadata
// typedef WavePluginInfo* (*wave_plugin_get_info_fn)(void);

// Required: called after load, receives host API. Return 0 on success.
// typedef int (*wave_plugin_init_fn)(const WaveHostAPI* api);

// Required: called before unload
// typedef void (*wave_plugin_shutdown_fn)(void);

// Optional: called when an event fires (alternative to subscribe)
// typedef void (*wave_plugin_on_event_fn)(WaveEventType event);

#ifdef __cplusplus
}
#endif
