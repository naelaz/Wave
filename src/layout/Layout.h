#pragma once

#include <string>
#include <vector>

namespace wave {

// Identifies a UI panel that can be shown/hidden
enum class PanelId {
    SidePanel,      // Library/Playlists/Queue panel on the left
    NowPlaying,     // Central now-playing area (title, artist, state)
    TransportBar,   // Progress bar + time
    TransportBtns,  // Prev/Play/Stop/Next buttons
    Visualizer,     // Spectrum / waveform visualizer region
    Count_          // sentinel — must be last
};

constexpr int PANEL_COUNT = static_cast<int>(PanelId::Count_);

// A named layout preset — snapshot of which panels are visible
struct LayoutPreset {
    std::wstring name;
    bool visible[PANEL_COUNT] = { true, true, true, true, true };
};

class Layout {
public:
    Layout();

    // Panel visibility
    bool isPanelVisible(PanelId id) const;
    void setPanelVisible(PanelId id, bool vis);
    void togglePanel(PanelId id);

    // Current side panel width (0 if hidden)
    float sidePanelWidth() const;

    // Presets
    const std::vector<LayoutPreset>& presets() const { return m_presets; }
    int activePreset() const { return m_activePreset; }
    void applyPreset(int index);
    int  saveCurrentAsPreset(const std::wstring& name);
    void deletePreset(int index);
    void resetToDefault();

    // Persistence — saves/loads presets + current visibility
    void save() const;
    void load();

    static constexpr float DEFAULT_PANEL_WIDTH = 300.0f;

private:
    bool m_visible[PANEL_COUNT]{};
    int m_activePreset = 0;
    std::vector<LayoutPreset> m_presets;

    static LayoutPreset makeDefault();
    static LayoutPreset makeMini();
    static std::wstring filePath();
};

} // namespace wave
