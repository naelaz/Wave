/*
 * Wave Sample Plugin
 * Demonstrates: metadata, init, commands, events, shutdown.
 */

#include <windows.h>
#include "../../sdk/wave_plugin_sdk.h"
#include <stdio.h>

/* ── State ────────────────────────────────────────────────── */

static const WaveHostAPI* g_api = NULL;
static int g_trackCount = 0;

/* ── Plugin info ──────────────────────────────────────────── */

static WavePluginInfo g_info = {
    WAVE_PLUGIN_API_VERSION,
    "com.wave.sample",
    "Sample Plugin",
    "Wave Team",
    "1.0.0",
    "Demonstrates the Wave plugin SDK: commands and events."
};

WAVE_EXPORT WavePluginInfo* wave_plugin_get_info(void) {
    return &g_info;
}

/* ── Commands ─────────────────────────────────────────────── */

static void cmd_show_now_playing(void* ud) {
    (void)ud;
    if (!g_api) return;

    WavePlaybackInfo info;
    g_api->getPlaybackInfo(&info);

    char buf[512];
    const char* state = "Stopped";
    if (info.state == 1) state = "Playing";
    else if (info.state == 2) state = "Paused";

    snprintf(buf, sizeof(buf),
        "[Sample Plugin] Now Playing:\n"
        "  State: %s\n"
        "  Position: %.1f / %.1f\n"
        "  Volume: %.0f%%\n"
        "  Tracks played this session: %d",
        state, info.position, info.duration, info.volume, g_trackCount);

    g_api->log("info", buf);

    /* Also show a message box */
    MessageBoxA(NULL, buf, "Wave Sample Plugin", MB_OK | MB_ICONINFORMATION);
}

static void cmd_toggle_pause(void* ud) {
    (void)ud;
    if (g_api) g_api->pause();
}

/* ── Events ───────────────────────────────────────────────── */

static void on_track_changed(WaveEventType event, void* ud) {
    (void)event;
    (void)ud;
    g_trackCount++;

    if (g_api) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Track changed! Total this session: %d", g_trackCount);
        g_api->log("info", buf);
    }
}

/* ── Lifecycle ────────────────────────────────────────────── */

WAVE_EXPORT int wave_plugin_init(const WaveHostAPI* api) {
    g_api = api;
    g_trackCount = 0;

    api->log("info", "Sample Plugin initializing...");

    /* Register commands — these appear in the Plugins menu */
    api->registerCommand("sample.now_playing", "Show Now Playing Info", cmd_show_now_playing, NULL);
    api->registerCommand("sample.toggle_pause", "Toggle Pause (plugin)", cmd_toggle_pause, NULL);

    /* Subscribe to track change events */
    api->subscribe(WAVE_EVENT_TRACK_CHANGED, on_track_changed, NULL);

    api->log("info", "Sample Plugin initialized successfully.");
    return 0;
}

WAVE_EXPORT void wave_plugin_shutdown(void) {
    if (g_api) g_api->log("info", "Sample Plugin shutting down.");
    g_api = NULL;
}

WAVE_EXPORT void wave_plugin_on_event(WaveEventType event) {
    /* This is an alternative to subscribe — called for ALL events */
    if (event == WAVE_EVENT_PLAYBACK_STATE && g_api) {
        WavePlaybackInfo info;
        g_api->getPlaybackInfo(&info);
        const char* states[] = { "Stopped", "Playing", "Paused" };
        const char* s = (info.state >= 0 && info.state <= 2) ? states[info.state] : "?";
        char buf[64];
        snprintf(buf, sizeof(buf), "Playback state: %s", s);
        g_api->log("info", buf);
    }
}
