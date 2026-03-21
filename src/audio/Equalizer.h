#pragma once

#include <string>
#include <array>

namespace wave {

class Engine;

// 10-band parametric EQ using mpv's lavfi equalizer filters
// Bands: 31Hz, 62Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz
class Equalizer {
public:
    static constexpr int BAND_COUNT = 10;
    static constexpr float MIN_GAIN = -12.0f; // dB
    static constexpr float MAX_GAIN = 12.0f;  // dB

    struct Preset {
        const wchar_t* name;
        std::array<float, BAND_COUNT> gains; // dB per band
    };

    Equalizer();

    // Get/set individual band gain in dB
    float bandGain(int band) const;
    void setBandGain(int band, float dB);

    // Get/set all bands at once
    const std::array<float, BAND_COUNT>& gains() const { return m_gains; }
    void setGains(const std::array<float, BAND_COUNT>& gains);

    // Presets
    static int presetCount();
    static const Preset& preset(int index);
    void applyPreset(int index);
    int currentPreset() const { return m_presetIdx; }

    // Enable/disable
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);

    // Build the mpv af filter string for current settings
    std::string buildFilterString() const;

    // Apply to engine (call after any change)
    void applyToEngine(Engine* engine);

    // Band frequency labels
    static const wchar_t* bandLabel(int band);
    static float bandFrequency(int band);

    // Serialization
    std::string serialize() const;
    void deserialize(const std::string& data);

private:
    std::array<float, BAND_COUNT> m_gains{}; // all 0 = flat
    bool m_enabled = false;
    int m_presetIdx = -1; // -1 = custom
};

} // namespace wave
