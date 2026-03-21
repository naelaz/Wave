#include "visualizer/Visualizer.h"
#include "audio/AudioCapture.h"
#include <cmath>
#include <algorithm>

namespace wave {

void Visualizer::update(bool isPlaying, double /*position*/, float dt) {
    if (m_mode == VisMode::Off) return;

    if (!isPlaying || !m_capture) {
        // Decay to zero
        for (int i = 0; i < BAR_COUNT; i++) {
            m_bars[i] *= std::max(0.0f, 1.0f - dt * 5.0f);
            if (m_bars[i] < 0.001f) m_bars[i] = 0.0f;
        }
        return;
    }

    // Read real spectrum from AudioCapture
    // AudioCapture outputs SPECTRUM_BINS (64), we downsample to BAR_COUNT (32)
    float raw[AudioCapture::SPECTRUM_BINS]{};
    m_capture->getSpectrum(raw, AudioCapture::SPECTRUM_BINS);

    // Downsample 64 bins → 32 bars by averaging pairs
    float targets[BAR_COUNT]{};
    int ratio = AudioCapture::SPECTRUM_BINS / BAR_COUNT;
    for (int i = 0; i < BAR_COUNT; i++) {
        float sum = 0;
        for (int j = 0; j < ratio; j++)
            sum += raw[i * ratio + j];
        targets[i] = sum / ratio;
    }

    // Smooth: fast attack, slower decay (looks better)
    float attackRate = dt * 18.0f; // quick response
    float decayRate  = dt * 5.0f;  // gentle fall

    for (int i = 0; i < BAR_COUNT; i++) {
        if (targets[i] > m_smoothed[i])
            m_smoothed[i] += (targets[i] - m_smoothed[i]) * std::min(attackRate, 1.0f);
        else
            m_smoothed[i] += (targets[i] - m_smoothed[i]) * std::min(decayRate, 1.0f);

        m_bars[i] = std::clamp(m_smoothed[i], 0.0f, 1.0f);
    }
}

} // namespace wave
