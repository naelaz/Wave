#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>

namespace wave {

class Engine;
class Library;
class PlaylistManager;

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
    bool init(HWND hwnd, Engine* engine, Library* library, PlaylistManager* playlists);
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

    // Scroll
    void scrollPanel(int rows);

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

    HWND m_hwnd = nullptr;
    Engine* m_engine = nullptr;
    Library* m_library = nullptr;
    PlaylistManager* m_playlists = nullptr;

    // UI state
    HitZone m_hover = HitZone::None;
    HitZone m_pressed = HitZone::None;
    PanelTab m_activeTab = PanelTab::Library;
    int m_viewedPlaylist = -1; // -1 = show playlist list; >=0 = show tracks
    int m_scrollOffset = 0;

    // Layout constants
    static constexpr float PANEL_WIDTH = 300.0f;
    static constexpr float ROW_HEIGHT = 40.0f;
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
};

} // namespace wave
