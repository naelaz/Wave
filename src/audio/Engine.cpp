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

bool Engine::init(HWND notifyWindow) {
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
    std::string msg = "Volume: " + std::to_string(static_cast<int>(m_volume)) + "%";
    log::info(msg);
}

double Engine::volume()   const { return m_volume; }
double Engine::position() const { return m_position; }
double Engine::duration() const { return m_duration; }
PlaybackState Engine::state() const { return m_state; }
const std::wstring& Engine::fileNameW() const { return m_fileName; }

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

            case MPV_EVENT_END_FILE:
                m_state = PlaybackState::Stopped;
                m_position = 0.0;
                log::info("Playback ended");
                if (m_trackEndCb) m_trackEndCb(m_trackEndCtx);
                break;

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
