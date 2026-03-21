#include "audio/Engine.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <string>
#include <cstring>
#include <cstdint>

// ── mpv C API type definitions ───────────────────────────────
// Minimal subset matching libmpv's stable public API.
// Defined here so we don't need mpv headers at build time.

struct mpv_handle;

enum mpv_format {
    MPV_FORMAT_NONE   = 0,
    MPV_FORMAT_STRING = 1,
    MPV_FORMAT_FLAG   = 3,
    MPV_FORMAT_INT64  = 4,
    MPV_FORMAT_DOUBLE = 5,
};

enum mpv_event_id {
    MPV_EVENT_NONE            = 0,
    MPV_EVENT_SHUTDOWN        = 1,
    MPV_EVENT_END_FILE        = 7,
    MPV_EVENT_FILE_LOADED     = 8,
    MPV_EVENT_PROPERTY_CHANGE = 22,
};

struct mpv_event_end_file {
    int reason;   // 0=eof, 2=stop, 3=quit, 4=error
    int error;
};

struct mpv_event_property {
    const char* name;
    mpv_format  format;
    void*       data;
};

struct mpv_event {
    mpv_event_id event_id;
    int          error;
    uint64_t     reply_userdata;
    void*        data;
};

// ── mpv function pointer types ───────────────────────────────

using pfn_mpv_create              = mpv_handle*(*)();
using pfn_mpv_initialize          = int(*)(mpv_handle*);
using pfn_mpv_terminate_destroy   = void(*)(mpv_handle*);
using pfn_mpv_set_option_string   = int(*)(mpv_handle*, const char*, const char*);
using pfn_mpv_command             = int(*)(mpv_handle*, const char**);
using pfn_mpv_set_property        = int(*)(mpv_handle*, const char*, mpv_format, void*);
using pfn_mpv_get_property        = int(*)(mpv_handle*, const char*, mpv_format, void*);
using pfn_mpv_observe_property    = int(*)(mpv_handle*, uint64_t, const char*, mpv_format);
using pfn_mpv_wait_event          = mpv_event*(*)(mpv_handle*, double);
using pfn_mpv_set_wakeup_callback = void(*)(mpv_handle*, void(*)(void*), void*);
using pfn_mpv_error_string        = const char*(*)(int);
using pfn_mpv_get_property_string = char*(*)(mpv_handle*, const char*);
using pfn_mpv_free                = void(*)(void*);

// ── Loaded function pointers ─────────────────────────────────

static struct MpvApi {
    HMODULE                     dll = nullptr;
    pfn_mpv_create              create = nullptr;
    pfn_mpv_initialize          initialize = nullptr;
    pfn_mpv_terminate_destroy   terminate_destroy = nullptr;
    pfn_mpv_set_option_string   set_option_string = nullptr;
    pfn_mpv_command             command = nullptr;
    pfn_mpv_set_property        set_property = nullptr;
    pfn_mpv_get_property        get_property = nullptr;
    pfn_mpv_observe_property    observe_property = nullptr;
    pfn_mpv_wait_event          wait_event = nullptr;
    pfn_mpv_set_wakeup_callback set_wakeup_callback = nullptr;
    pfn_mpv_error_string        error_string = nullptr;
    pfn_mpv_get_property_string get_property_string = nullptr;
    pfn_mpv_free                free_fn = nullptr;
} mpv;

static bool loadMpvDll() {
    mpv.dll = LoadLibraryW(L"libmpv-2.dll");
    if (!mpv.dll) mpv.dll = LoadLibraryW(L"mpv-2.dll");
    if (!mpv.dll) {
        wave::log::error("Could not find libmpv-2.dll or mpv-2.dll next to Wave.exe");
        return false;
    }

#define LOAD(name) \
    mpv.name = reinterpret_cast<pfn_mpv_##name>(GetProcAddress(mpv.dll, "mpv_" #name)); \
    if (!mpv.name) { wave::log::error("Missing mpv export: mpv_" #name); return false; }

    LOAD(create)
    LOAD(initialize)
    LOAD(terminate_destroy)
    LOAD(set_option_string)
    LOAD(command)
    LOAD(set_property)
    LOAD(get_property)
    LOAD(observe_property)
    LOAD(wait_event)
    LOAD(set_wakeup_callback)
    LOAD(error_string)

    // Optional: these may not exist in very old mpv builds
    mpv.get_property_string = reinterpret_cast<pfn_mpv_get_property_string>(
        GetProcAddress(mpv.dll, "mpv_get_property_string"));
    mpv.free_fn = reinterpret_cast<pfn_mpv_free>(
        GetProcAddress(mpv.dll, "mpv_free"));

#undef LOAD

    wave::log::info("libmpv loaded successfully");
    return true;
}

static void unloadMpvDll() {
    if (mpv.dll) {
        FreeLibrary(mpv.dll);
        mpv = {};
    }
}

// ── Wakeup callback (called from mpv's internal thread) ──────

static void onMpvWakeup(void* ctx) {
    PostMessageW(static_cast<HWND>(ctx), wave::Engine::WM_MPV_WAKEUP, 0, 0);
}

// ── Engine implementation ────────────────────────────────────

namespace wave {

bool Engine::init(HWND notifyWindow, const AudioSettings& audioSettings) {
    m_notifyWindow = notifyWindow;

    if (!loadMpvDll()) return false;

    m_mpv = mpv.create();
    if (!m_mpv) {
        log::error("mpv_create failed");
        return false;
    }

    auto* ctx = static_cast<mpv_handle*>(m_mpv);

    // Audio-only: no video output
    mpv.set_option_string(ctx, "vo", "null");
    mpv.set_option_string(ctx, "vid", "no");
    mpv.set_option_string(ctx, "terminal", "no");
    mpv.set_option_string(ctx, "msg-level", "all=no");

    // ── High-quality audio defaults ──
    // Pass audio through without unnecessary resampling or format conversion
    mpv.set_option_string(ctx, "audio-samplerate", "0");       // 0 = keep source sample rate
    mpv.set_option_string(ctx, "audio-format", "0");            // 0 = keep source format
    mpv.set_option_string(ctx, "audio-channels", "auto-safe");  // match source channels
    mpv.set_option_string(ctx, "audio-normalize-downmix", "no");// no loudness normalization
    mpv.set_option_string(ctx, "audio-pitch-correction", "yes");// clean pitch on speed change
    mpv.set_option_string(ctx, "audio-swresample-o", "");       // no extra resampler options
    // When resampling IS needed, use highest quality
    mpv.set_option_string(ctx, "af", "");                       // no audio filters by default

    // Audio backend
    const char* ao = audioSettings.backendStr();
    if (ao[0]) {
        mpv.set_option_string(ctx, "ao", ao);
        log::info(std::string("Audio backend: ") + ao);
    }

    // Audio device
    if (!audioSettings.deviceId.empty()) {
        mpv.set_option_string(ctx, "audio-device", audioSettings.deviceId.c_str());
        log::info("Audio device: " + audioSettings.deviceId);
    }

    // Gapless playback
    mpv.set_option_string(ctx, "gapless-audio", audioSettings.gapless ? "yes" : "no");

    // ReplayGain
    mpv.set_option_string(ctx, "replaygain", audioSettings.replayGainStr());
    if (audioSettings.replayGain != ReplayGainMode::Off) {
        std::string preamp = std::to_string(audioSettings.replayGainPreamp);
        mpv.set_option_string(ctx, "replaygain-preamp", preamp.c_str());
    }

    if (mpv.initialize(ctx) < 0) {
        log::error("mpv_initialize failed");
        mpv.terminate_destroy(ctx);
        m_mpv = nullptr;
        return false;
    }

    // Observe properties for state tracking
    mpv.observe_property(ctx, 0, "pause",    MPV_FORMAT_FLAG);
    mpv.observe_property(ctx, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv.observe_property(ctx, 0, "duration", MPV_FORMAT_DOUBLE);

    // Post WM_MPV_WAKEUP to our window when mpv has events
    mpv.set_wakeup_callback(ctx, onMpvWakeup, notifyWindow);

    log::info("Audio engine initialized");
    return true;
}

void Engine::shutdown() {
    if (m_mpv) {
        mpv.terminate_destroy(static_cast<mpv_handle*>(m_mpv));
        m_mpv = nullptr;
        m_state = PlaybackState::Stopped;
        log::info("Audio engine shut down");
    }
    unloadMpvDll();
}

bool Engine::loadFile(std::string_view path) {
    if (!m_mpv) {
        log::warn("Cannot load file: audio engine not initialized");
        return false;
    }

    if (path.empty()) return false;
    std::string p(path);
    const char* cmd[] = { "loadfile", p.c_str(), nullptr };
    int err = mpv.command(static_cast<mpv_handle*>(m_mpv), cmd);
    if (err < 0) {
        std::string msg = "Failed to load: ";
        msg += mpv.error_string(err);
        log::error(msg);
        return false;
    }

    // Extract filename only (strip directory path)
    std::wstring widePath = platform::toWide(path);
    auto slash = widePath.find_last_of(L"\\/");
    m_fileName = (slash != std::wstring::npos) ? widePath.substr(slash + 1) : widePath;

    std::string msg = "Loading: ";
    msg += path;
    log::info(msg);
    return true;
}

void Engine::togglePause() {
    if (!m_mpv || m_state == PlaybackState::Stopped) return;
    const char* cmd[] = { "cycle", "pause", nullptr };
    mpv.command(static_cast<mpv_handle*>(m_mpv), cmd);
}

void Engine::setPaused(bool paused) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    int flag = paused ? 1 : 0;
    mpv.set_property(ctx, "pause", MPV_FORMAT_FLAG, &flag);
}

void Engine::stop() {
    if (!m_mpv) return;
    const char* cmd[] = { "stop", nullptr };
    mpv.command(static_cast<mpv_handle*>(m_mpv), cmd);
    m_state = PlaybackState::Stopped;
    m_position = 0.0;
    m_duration = 0.0;
    m_fileName.clear();
    log::info("Playback stopped");
}

void Engine::seekRelative(double seconds) {
    if (!m_mpv || m_state == PlaybackState::Stopped) return;
    std::string sec = std::to_string(seconds);
    const char* cmd[] = { "seek", sec.c_str(), "relative", nullptr };
    mpv.command(static_cast<mpv_handle*>(m_mpv), cmd);
    std::string msg = "Seek: ";
    msg += (seconds >= 0 ? "+" : "");
    msg += std::to_string(static_cast<int>(seconds));
    msg += "s";
    log::info(msg);
}

void Engine::seekAbsolute(double seconds) {
    if (!m_mpv || m_state == PlaybackState::Stopped) return;
    if (seconds < 0.0) seconds = 0.0;
    if (m_duration > 0.0 && seconds > m_duration) seconds = m_duration;
    std::string sec = std::to_string(seconds);
    const char* cmd[] = { "seek", sec.c_str(), "absolute", nullptr };
    mpv.command(static_cast<mpv_handle*>(m_mpv), cmd);
}

void Engine::setVolume(double vol) {
    if (!m_mpv) return;
    if (vol < 0.0) vol = 0.0;
    if (vol > 100.0) vol = 100.0;
    m_volume = vol;
    mpv.set_property(static_cast<mpv_handle*>(m_mpv),
                     "volume", MPV_FORMAT_DOUBLE, &m_volume);
}

double Engine::volume()   const { return m_volume; }
double Engine::position() const { return m_position; }
double Engine::duration() const { return m_duration; }
PlaybackState Engine::state() const { return m_state; }
const std::wstring& Engine::fileNameW() const { return m_fileName; }

// ── Runtime audio settings ───────────────────────────────────

void Engine::setGapless(bool enabled) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    const char* val = enabled ? "yes" : "no";
    mpv.set_property(ctx, "gapless-audio", MPV_FORMAT_STRING, &val);
    log::info(std::string("Gapless: ") + (enabled ? "on" : "off"));
}

void Engine::setReplayGain(ReplayGainMode mode) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    const char* val = "no";
    if (mode == ReplayGainMode::Track) val = "track";
    else if (mode == ReplayGainMode::Album) val = "album";
    mpv.set_property(ctx, "replaygain", MPV_FORMAT_STRING, &val);
    log::info(std::string("ReplayGain: ") + val);
}

void Engine::setReplayGainPreamp(double db) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    mpv.set_property(ctx, "replaygain-preamp", MPV_FORMAT_DOUBLE, &db);
    log::info("ReplayGain preamp: " + std::to_string(db) + " dB");
}

void Engine::setAudioDevice(const std::string& deviceId) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    const char* val = deviceId.c_str();
    mpv.set_property(ctx, "audio-device", MPV_FORMAT_STRING, &val);
    log::info("Audio device set to: " + deviceId);
}

void Engine::setAudioFilter(const std::string& filterStr) {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    // Must use set_property (not set_option_string) for runtime changes
    const char* val = filterStr.c_str();
    int err = mpv.set_property(ctx, "af", MPV_FORMAT_STRING, &val);
    if (err < 0) {
        log::warn("Failed to set audio filter: " + std::string(mpv.error_string(err)));
    } else {
        log::info("Audio filter: " + (filterStr.empty() ? "(cleared)" : filterStr));
    }
}

std::vector<Engine::AudioDevice> Engine::getAudioDevices() const {
    std::vector<AudioDevice> devices;
    if (!m_mpv || !mpv.get_property_string) return devices;

    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    char* list = mpv.get_property_string(ctx, "audio-device-list");
    if (!list) return devices;

    // mpv returns JSON: [{"name":"...","description":"..."},...]
    std::string json(list);
    if (mpv.free_fn) mpv.free_fn(list);

    // Simple JSON array parser for [{name,description},...] objects
    size_t pos = 0;
    while (pos < json.size()) {
        auto nameKey = json.find("\"name\"", pos);
        if (nameKey == std::string::npos) break;
        auto nameStart = json.find('"', nameKey + 6);
        if (nameStart == std::string::npos) break;
        nameStart++;
        auto nameEnd = json.find('"', nameStart);
        if (nameEnd == std::string::npos) break;

        auto descKey = json.find("\"description\"", nameEnd);
        if (descKey == std::string::npos) break;
        auto descStart = json.find('"', descKey + 13);
        if (descStart == std::string::npos) break;
        descStart++;
        auto descEnd = json.find('"', descStart);
        if (descEnd == std::string::npos) break;

        AudioDevice dev;
        dev.id = json.substr(nameStart, nameEnd - nameStart);
        dev.name = json.substr(descStart, descEnd - descStart);
        devices.push_back(std::move(dev));
        pos = descEnd + 1;
    }

    return devices;
}

std::string Engine::currentAudioDevice() const {
    if (!m_mpv || !mpv.get_property_string) return {};
    auto* ctx = static_cast<mpv_handle*>(m_mpv);
    char* val = mpv.get_property_string(ctx, "audio-device");
    if (!val) return {};
    std::string result(val);
    if (mpv.free_fn) mpv.free_fn(val);
    return result;
}

void Engine::processEvents() {
    if (!m_mpv) return;
    auto* ctx = static_cast<mpv_handle*>(m_mpv);

    for (;;) {
        mpv_event* ev = mpv.wait_event(ctx, 0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id) {
            case MPV_EVENT_FILE_LOADED:
                m_state = PlaybackState::Playing;
                log::info("Playback started");
                break;

            case MPV_EVENT_END_FILE: {
                auto* ef = static_cast<mpv_event_end_file*>(ev->data);
                int reason = ef ? ef->reason : -1;
                m_state = PlaybackState::Stopped;
                m_position = 0.0;
                // Only auto-advance on natural EOF (reason 0).
                // Reason 2 (stop) fires when loadfile replaces the current
                // track — calling the callback would double-skip.
                if (reason == 0) {
                    log::info("Track finished (EOF)");
                    if (m_trackEndCb) m_trackEndCb(m_trackEndCtx);
                } else {
                    log::info("Playback ended (reason " + std::to_string(reason) + ")");
                }
                break;
            }

            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = static_cast<mpv_event_property*>(ev->data);
                if (!prop || !prop->name || !prop->data) break;

                if (std::strcmp(prop->name, "pause") == 0 &&
                    prop->format == MPV_FORMAT_FLAG)
                {
                    bool paused = *static_cast<int*>(prop->data) != 0;
                    if (m_state != PlaybackState::Stopped) {
                        m_state = paused ? PlaybackState::Paused
                                         : PlaybackState::Playing;
                        log::info(paused ? "Playback paused" : "Playback resumed");
                    }
                }
                else if (std::strcmp(prop->name, "time-pos") == 0 &&
                         prop->format == MPV_FORMAT_DOUBLE)
                {
                    m_position = *static_cast<double*>(prop->data);
                }
                else if (std::strcmp(prop->name, "duration") == 0 &&
                         prop->format == MPV_FORMAT_DOUBLE)
                {
                    m_duration = *static_cast<double*>(prop->data);
                }
                break;
            }

            case MPV_EVENT_SHUTDOWN:
                m_state = PlaybackState::Stopped;
                break;

            default:
                break;
        }
    }
}

} // namespace wave
