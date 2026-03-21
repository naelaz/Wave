#pragma once

#include <Windows.h>

namespace wave {

class App;
class Engine;
class Renderer;
class Library;

struct Settings;

class MainWindow {
public:
    bool create(HINSTANCE hInstance, App* app, const Settings& settings);
    void setRenderer(Renderer* r);
    void refreshPluginMenu();
    void refreshAudioMenu();
    void show(const Settings& settings);
    void saveWindowState(Settings& settings) const;
    HWND handle() const { return m_hwnd; }

private:
    struct WndData {
        App*         app;
        Engine*      engine;
        Renderer*    renderer;
        Library*     library;
        MainWindow*  mainWindow;
        bool         draggingBar = false;
        int          contextRow = -1; // row index for right-click context
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static void createMenu(HWND hwnd);

    static constexpr const wchar_t* CLASS_NAME = L"WaveMainWindow";
    static constexpr int DEFAULT_WIDTH  = 1100;
    static constexpr int DEFAULT_HEIGHT = 700;
    static constexpr UINT_PTR TIMER_REDRAW = 1;
    static constexpr UINT TIMER_INTERVAL_MS = 30;

    // Menu command IDs
    static constexpr UINT CMD_OPEN_FILE     = 101;
    static constexpr UINT CMD_OPEN_FOLDER   = 102;
    static constexpr UINT CMD_ADD_FOLDER    = 105;
    static constexpr UINT CMD_NEW_PLAYLIST  = 103;
    static constexpr UINT CMD_DEL_PLAYLIST  = 104;

    // View menu command IDs
    static constexpr UINT CMD_TOGGLE_PANEL  = 111;
    static constexpr UINT CMD_TOGGLE_NP     = 112;
    static constexpr UINT CMD_TOGGLE_BAR    = 113;
    static constexpr UINT CMD_TOGGLE_BTNS   = 114;
    static constexpr UINT CMD_PRESET_BASE   = 120; // 120+ = apply preset N
    static constexpr UINT CMD_SAVE_PRESET   = 140;
    static constexpr UINT CMD_RESET_LAYOUT  = 141;

    // Audio menu command IDs
    static constexpr UINT CMD_AO_AUTO      = 601;
    static constexpr UINT CMD_AO_WASAPI    = 602;
    static constexpr UINT CMD_AO_DSOUND    = 603;
    static constexpr UINT CMD_GAPLESS_TOG  = 610;
    static constexpr UINT CMD_RG_OFF       = 611;
    static constexpr UINT CMD_RG_TRACK     = 612;
    static constexpr UINT CMD_RG_ALBUM     = 613;
    static constexpr UINT CMD_RG_PREAMP_UP = 614;
    static constexpr UINT CMD_RG_PREAMP_DN = 615;
    static constexpr UINT CMD_START_PAUSED = 620;
    static constexpr UINT CMD_DEVICE_BASE  = 630; // 630+ = audio device N
    static constexpr UINT CMD_DEVICE_MAX   = 660; // up to 30 devices

    // Sort command IDs
    static constexpr UINT CMD_SORT_TITLE    = 501;
    static constexpr UINT CMD_SORT_ARTIST   = 502;
    static constexpr UINT CMD_SORT_ALBUM    = 503;
    static constexpr UINT CMD_SORT_TRACKNUM = 504;
    static constexpr UINT CMD_SORT_FILENAME = 505;
    static constexpr UINT CMD_SORT_TOGGLE   = 506; // toggle asc/desc

    // Visualizer command IDs
    static constexpr UINT CMD_VIS_OFF      = 145;
    static constexpr UINT CMD_VIS_SPECTRUM  = 146;
    static constexpr UINT CMD_TOGGLE_VIS   = 147;

    // Cover Flow
    static constexpr UINT CMD_TOGGLE_CF     = 148;

    // Playback mode command IDs
    static constexpr UINT CMD_SHUFFLE_TOG   = 701;
    static constexpr UINT CMD_REPEAT_CYCLE  = 702;

    // EQ command IDs
    static constexpr UINT CMD_EQ_TOGGLE     = 710;
    static constexpr UINT CMD_EQ_PRESET_BASE = 720; // 720+ = EQ preset N
    static constexpr UINT CMD_EQ_BAND_BASE  = 750; // 750+ = band adjustments (future)

    // Theme menu command IDs
    static constexpr UINT CMD_THEME_BASE    = 150; // 150+ = theme preset N
    static constexpr UINT CMD_ACCENT_BASE   = 170; // 170+ = accent N
    static constexpr UINT CMD_THEME_RESET   = 190;

    // Help menu
    static constexpr UINT CMD_ABOUT         = 191;
    static constexpr UINT CMD_OPEN_LOG      = 192;
    static constexpr UINT CMD_OPEN_DATA_DIR = 193;

    // Plugin menu command IDs
    static constexpr UINT CMD_PLUGIN_INFO   = 195;
    static constexpr UINT CMD_PLUGIN_CMD_BASE = 400; // 400+ = plugin command N

    // Context menu command IDs
    static constexpr UINT CMD_CTX_PLAY      = 201;
    static constexpr UINT CMD_CTX_QUEUE     = 202;
    static constexpr UINT CMD_CTX_PL_BASE   = 300; // 300+ = add to playlist N

    HWND m_hwnd = nullptr;
    WndData m_wndData{};
};

} // namespace wave
