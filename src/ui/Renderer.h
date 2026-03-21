#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>

namespace wave {

class Engine;
class Library;
class PlaylistManager;
class Layout;
class Theme;
class Visualizer;
class AlbumArt;
class WaveformCache;
class CoverFlow;

enum class HitZone {
    None,
    ProgressBar,
    PlayPause,
    Stop,
    Prev,
    Next,
    PanelRow,      // a row in the active panel (library/playlist/queue)
    TabLibrary,
    TabPlaylists,
    TabQueue
};

enum class PanelTab { Library, Playlists, Queue };

class Renderer {
public:
    bool init(HWND hwnd, Engine* engine, Library* library, PlaylistManager* playlists, Layout* layout, Theme* theme, Visualizer* visualizer, AlbumArt* albumArt, WaveformCache* waveform);

    // Force brushes to update from current theme colors
    void applyTheme();
    void shutdown();
    void render();
    void resize();

    // Hit testing
    HitZone hitTest(float x, float y) const;
    float progressBarFraction(float x) const;
    int panelRowAt(float y) const;

    // UI state
    void setHover(HitZone zone)   { m_hover = zone; }
    void setPressed(HitZone zone) { m_pressed = zone; }
    HitZone pressed() const       { return m_pressed; }

    // Tabs
    PanelTab activeTab() const { return m_activeTab; }
    void setActiveTab(PanelTab tab) { m_activeTab = tab; m_scrollOffset = 0; }

    // Playlist navigation in the Playlists tab
    int viewedPlaylist() const { return m_viewedPlaylist; }
    void setViewedPlaylist(int idx) { m_viewedPlaylist = idx; m_scrollOffset = 0; }

    // Scrub preview
    void beginScrub(float anchorY);
    void updateScrub(float mx, float my);
    void endScrub();
    bool isScrubPreview() const { return m_scrubPreview; }
    float scrubFraction() const { return m_scrubFrac; }

    // Cover Flow
    void setCoverFlow(CoverFlow* cf) { m_coverFlow = cf; }
    void setCoverFlowMode(bool on) { m_coverFlowMode = on; }
    bool isCoverFlowMode() const { return m_coverFlowMode; }
    int coverFlowHitTest(float x, float y) const; // returns album index or -1

    // Playback mode indicators
    void setShuffleRepeat(bool shuffle, int repeatMode) { m_showShuffle = shuffle; m_showRepeat = repeatMode; }

    // Scroll
    void scrollPanel(int rows);
    void setScrollOffset(int offset) { m_scrollOffset = offset; }

private:
    void createDeviceResources();
    void discardDeviceResources();
    void computeLayout();
    void formatTime(double seconds, wchar_t* buf, int bufLen) const;
    void drawButton(D2D1_RECT_F rect, const wchar_t* label, bool enabled, HitZone zone);
    void drawPanel();
    void drawTabs(float panelRight, float tabY);
    void drawLibraryList(float left, float right, float top, float bottom);
    void drawPlaylistList(float left, float right, float top, float bottom);
    void drawPlaylistTracks(float left, float right, float top, float bottom);
    void drawQueueList(float left, float right, float top, float bottom);
    void drawTrackRow(const struct TrackInfo& track, float left, float right, float y,
                      bool isPlaying, bool isActive);
    void drawVisualizer();
    void drawNowPlaying(float centerY, float w, float margin);
    void drawWelcome(float w, float h);
    void drawScrubPreview();
    void drawCoverFlow();

    HWND m_hwnd = nullptr;
    Engine* m_engine = nullptr;
    Library* m_library = nullptr;
    PlaylistManager* m_playlists = nullptr;
    Layout* m_layout = nullptr;
    Theme* m_theme = nullptr;
    Visualizer* m_visualizer = nullptr;
    AlbumArt* m_albumArt = nullptr;
    WaveformCache* m_waveform = nullptr;
    CoverFlow* m_coverFlow = nullptr;
    bool m_coverFlowMode = false;
    struct CfHitRect { int albumIndex; D2D1_RECT_F rect; };
    std::vector<CfHitRect> m_cfHitRects; // rebuilt each frame during drawCoverFlow

    // Scrub preview state
    bool m_scrubPreview = false;    // true when in upward-drag preview mode
    bool m_scrubActive = false;     // true while dragging the bar at all
    float m_scrubFrac = 0;          // horizontal position (0–1) during scrub
    float m_scrubAnchorY = 0;       // Y where mouse first clicked the bar
    float m_scrubAlpha = 0.0f;      // animation: 0=hidden, 1=fully visible
    float m_scrubMouseY = 0.0f;     // current mouse Y during drag

    // UI state
    HitZone m_hover = HitZone::None;
    HitZone m_pressed = HitZone::None;
    PanelTab m_activeTab = PanelTab::Library;
    int m_viewedPlaylist = -1; // -1 = show playlist list; >=0 = show tracks
    int m_scrollOffset = 0;
    bool m_showShuffle = false;
    int m_showRepeat = 0; // 0=off, 1=all, 2=one

    // Layout constants
    static constexpr float PANEL_WIDTH = 300.0f;
    static constexpr float ROW_HEIGHT = 40.0f;
    static constexpr float HEADER_HEIGHT = 32.0f;
    static constexpr float TAB_HEIGHT = 28.0f;
    static constexpr float PAD = 8.0f;

    // Cached layout rects
    D2D1_RECT_F m_progressBarRect{};
    D2D1_RECT_F m_btnPrev{};
    D2D1_RECT_F m_btnPlayPause{};
    D2D1_RECT_F m_btnStop{};
    D2D1_RECT_F m_btnNext{};
    D2D1_RECT_F m_panelRect{};
    D2D1_RECT_F m_tabLibrary{};
    D2D1_RECT_F m_tabPlaylists{};
    D2D1_RECT_F m_tabQueue{};
    float m_listTop = 0;
    float m_listBottom = 0;
    float m_playerLeft = 0;
    float m_libListTop = 0; // actual row start in library (after search bar)
    D2D1_RECT_F m_vizRect{};

    // Direct2D
    ID2D1Factory*          m_d2dFactory = nullptr;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    ID2D1SolidColorBrush*  m_textBrush = nullptr;
    ID2D1SolidColorBrush*  m_dimBrush = nullptr;
    ID2D1SolidColorBrush*  m_accentBrush = nullptr;
    ID2D1SolidColorBrush*  m_bgBrush = nullptr;
    ID2D1SolidColorBrush*  m_barBgBrush = nullptr;
    ID2D1SolidColorBrush*  m_btnBrush = nullptr;
    ID2D1SolidColorBrush*  m_btnHoverBrush = nullptr;
    ID2D1SolidColorBrush*  m_btnPressBrush = nullptr;
    ID2D1SolidColorBrush*  m_panelBgBrush = nullptr;
    ID2D1SolidColorBrush*  m_rowPlayingBrush = nullptr;
    ID2D1SolidColorBrush*  m_tabActiveBrush = nullptr;

    // DirectWrite
    IDWriteFactory*    m_dwFactory = nullptr;
    IDWriteTextFormat* m_titleFormat = nullptr;
    IDWriteTextFormat* m_bodyFormat = nullptr;
    IDWriteTextFormat* m_btnFormat = nullptr;
    IDWriteTextFormat* m_listFormat = nullptr;
    IDWriteTextFormat* m_listSmallFormat = nullptr;
    IDWriteTextFormat* m_iconFormat = nullptr;
    IDWriteTextFormat* m_badgeFormat = nullptr; // small format badge
};

} // namespace wave
