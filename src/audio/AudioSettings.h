#pragma once

#include <string>

namespace wave {

// Audio output backend
enum class AudioBackend {
    Auto,       // mpv default (usually wasapi)
    WASAPI,
    DirectSound,
};

// ReplayGain mode
enum class ReplayGainMode {
    Off,        // no replaygain
    Track,      // per-track normalization
    Album,      // per-album normalization
};

// Audio preferences — applied at engine init time.
// Changing most of these requires restarting the engine.
struct AudioSettings {
    AudioBackend backend = AudioBackend::Auto;
    std::string  deviceId;        // empty = default device

    bool         gapless = true;  // gapless playback between tracks
    ReplayGainMode replayGain = ReplayGainMode::Off;
    double       replayGainPreamp = 0.0; // dB, applied on top of RG tags

    bool         startPaused = false;
    bool         exclusiveMode = false; // WASAPI exclusive (future)
    int          crossfadeMs = 0;       // crossfade in ms (0=off, future)

    // Convert to mpv option strings
    const char* backendStr() const {
        switch (backend) {
            case AudioBackend::WASAPI:      return "wasapi";
            case AudioBackend::DirectSound: return "dsound";
            default:                        return "";
        }
    }

    const char* replayGainStr() const {
        switch (replayGain) {
            case ReplayGainMode::Track: return "track";
            case ReplayGainMode::Album: return "album";
            default:                    return "no";
        }
    }
};

} // namespace wave
