#pragma once

#include <Windows.h>

namespace wave {

class App;
class Engine;
class Renderer;
class Library;

class MainWindow {
public:
    bool create(HINSTANCE hInstance, App* app);
    void setRenderer(Renderer* r);
    void show();
    HWND handle() const { return m_hwnd; }

private:
    struct WndData {
        App*      app;
        Engine*   engine;
        Renderer* renderer;
        Library*  library;
        bool      draggingBar = false;
        int       contextRow = -1; // row index for right-click context
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
    static constexpr UINT CMD_NEW_PLAYLIST  = 103;
    static constexpr UINT CMD_DEL_PLAYLIST  = 104;

    // Context menu command IDs
    static constexpr UINT CMD_CTX_PLAY      = 201;
    static constexpr UINT CMD_CTX_QUEUE     = 202;
    static constexpr UINT CMD_CTX_PL_BASE   = 300; // 300+ = add to playlist N

    HWND m_hwnd = nullptr;
    WndData m_wndData{};
};

} // namespace wave
