#pragma once

#include <Windows.h>
#include <string>
#include <string_view>

namespace wave {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

class Engine {
public:
    // Window message posted when mpv has events ready
    static constexpr UINT WM_MPV_WAKEUP = WM_USER + 1;

    bool init(HWND notifyWindow);
    void shutdown();

    bool loadFile(std::string_view path);
    void togglePause();
    void stop();
    void seekRelative(double seconds);
    void seekAbsolute(double seconds);
    void setVolume(double vol);

    double volume() const;
    double position() const;
    double duration() const;
    PlaybackState state() const;
    const std::wstring& fileNameW() const;

    // Callback for track-end notification (auto-advance)
    using TrackEndCallback = void(*)(void* ctx);
    void setTrackEndCallback(TrackEndCallback cb, void* ctx) {
        m_trackEndCb = cb;
        m_trackEndCtx = ctx;
    }

    // Process pending mpv events. Call in response to WM_MPV_WAKEUP.
    void processEvents();

private:
    void* m_mpv = nullptr;
    HWND m_notifyWindow = nullptr;
    PlaybackState m_state = PlaybackState::Stopped;
    double m_volume = 100.0;
    double m_position = 0.0;
    double m_duration = 0.0;
    std::wstring m_fileName;
    TrackEndCallback m_trackEndCb = nullptr;
    void* m_trackEndCtx = nullptr;
};

} // namespace wave
