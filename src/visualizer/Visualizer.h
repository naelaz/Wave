#pragma once

#include <array>

namespace wave {

class AudioCapture;

enum class VisMode {
    Off,
    Spectrum,
};

class Visualizer {
public:
    static constexpr int BAR_COUNT = 32;

    void setMode(VisMode mode) { m_mode = mode; }
    VisMode mode() const { return m_mode; }

    void setCapture(AudioCapture* cap) { m_capture = cap; }

    // Call once per frame. isPlaying drives decay behavior.
    void update(bool isPlaying, double position, float dt);

    // Current bar heights (0.0–1.0), ready for rendering
    const std::array<float, BAR_COUNT>& bars() const { return m_bars; }

private:
    VisMode m_mode = VisMode::Off;
    AudioCapture* m_capture = nullptr;
    std::array<float, BAR_COUNT> m_bars{};
    std::array<float, BAR_COUNT> m_smoothed{};
};

} // namespace wave
