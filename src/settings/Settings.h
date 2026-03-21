#pragma once

#include <string>

namespace wave {

struct Settings {
    // Window state
    int windowX      = -1; // -1 = use default (centered)
    int windowY      = -1;
    int windowWidth  = 1100;
    int windowHeight = 700;
    bool maximized   = true; // fullscreen by default

    // Audio
    double volume = 100.0; // no digital attenuation — use OS volume control

    // Library
    std::wstring lastFolder; // reopen on startup if non-empty

    // Theme
    std::wstring themePreset = L"dark"; // preset id
    int themeAccent = 0;                // accent index (0 = preset default)

    // Visualizer
    int visualizerMode = 1; // 0=off, 1=spectrum

    // Audio
    int audioBackend = 0;       // 0=auto, 1=wasapi, 2=dsound
    std::string audioDevice;    // empty = default
    bool gapless = true;
    int replayGain = 0;         // 0=off, 1=track, 2=album
    double replayGainPreamp = 0.0;
    bool startPaused = false;   // start playback paused
    int crossfadeMs = 0;        // crossfade in ms (0=off, future use)
    int repeatMode = 0;         // 0=off, 1=all, 2=one
    bool shuffle = false;
    std::string eqData;         // serialized EQ state
    bool coverFlowMode = false; // true = show cover flow instead of now-playing

    // Load from %APPDATA%\Wave\settings.json. Missing/corrupt → keeps defaults.
    void load();

    // Save to %APPDATA%\Wave\settings.json.
    void save() const;

    // Returns true if running in portable mode (portable.txt next to exe)
    static bool isPortable();

    // Base data directory (%APPDATA%\Wave or ./data in portable mode)
    static std::wstring dataDir();

private:
    static std::wstring filePath();
};

} // namespace wave
