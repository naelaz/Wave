#pragma once

#include <string>
#include <vector>

namespace wave {

// RGBA color stored as 4 floats (0.0–1.0), matching D2D1::ColorF
struct ThemeColor {
    float r = 0, g = 0, b = 0, a = 1.0f;
    constexpr ThemeColor() = default;
    constexpr ThemeColor(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}
};

// All colors the renderer needs — no hardcoded values outside this struct
struct ThemeColors {
    ThemeColor background;
    ThemeColor panelBg;
    ThemeColor text;
    ThemeColor textDim;
    ThemeColor accent;
    ThemeColor barBg;         // progress bar background track
    ThemeColor btnBg;
    ThemeColor btnHover;
    ThemeColor btnPressed;
    ThemeColor rowPlaying;
    ThemeColor tabActive;
    ThemeColor clearColor;    // full-window clear color
};

struct ThemePreset {
    std::wstring id;
    std::wstring name;
    ThemeColors colors;
};

// Accent color choices
struct AccentOption {
    std::wstring name;
    ThemeColor color;
};

class Theme {
public:
    Theme();

    // Active colors — renderer reads these
    const ThemeColors& colors() const { return m_active; }

    // Presets
    const std::vector<ThemePreset>& presets() const { return m_presets; }
    int activePresetIndex() const { return m_presetIdx; }
    const std::wstring& activePresetId() const;
    void applyPreset(int index);
    void applyPresetById(const std::wstring& id);

    // Accent override
    const std::vector<AccentOption>& accentOptions() const { return m_accents; }
    int activeAccentIndex() const { return m_accentIdx; }
    void setAccent(int index);

    // Reset
    void resetToDefault();

    // Persistence helpers — called by Settings
    std::wstring presetId() const;
    int accentIndex() const { return m_accentIdx; }

private:
    void rebuildActive();

    std::vector<ThemePreset> m_presets;
    std::vector<AccentOption> m_accents;
    int m_presetIdx = 0;
    int m_accentIdx = 0; // 0 = use preset default
    ThemeColors m_active;
};

} // namespace wave
