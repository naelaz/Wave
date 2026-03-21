#pragma once

// ── Wave Plugin API ──────────────────────────────────────────
//
// Architecture:
//   Core (Engine, Library, Playlists, Settings)
//       ↕ PluginHost (manages lifecycle, routes events)
//       ↕ WaveHostAPI (C ABI table, passed to each plugin DLL)
//   Plugins (native DLLs exporting C functions)
//
// Plugin DLL exports:
//   wave_plugin_get_info()  → returns WavePluginInfo*
//   wave_plugin_init(api)   → receives WaveHostAPI*, returns 0 on success
//   wave_plugin_shutdown()  → cleanup before unload
//   wave_plugin_on_event(e) → optional event handler
//
// See PluginTypes.h for the C ABI definitions.
// See PluginHost.h for the host-side loader and event dispatcher.

#include "plugin/PluginTypes.h"
