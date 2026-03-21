#include "window/MainWindow.h"
#include "platform/Win32Helpers.h"
#include "core/Log.h"
#include "app/App.h"
#include "audio/Engine.h"
#include "ui/Renderer.h"
#include "library/Library.h"
#include "playlist/PlaylistManager.h"
#include "settings/Settings.h"
#include "layout/Layout.h"
#include "theme/Theme.h"
#include "visualizer/Visualizer.h"
#include "plugin/PluginHost.h"
#include "resources/resource.h"

#include "audio/AudioSettings.h"
#include "audio/Equalizer.h"

#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>

namespace wave {

void MainWindow::createMenu(HWND hwnd) {
    HMENU menuBar  = CreateMenu();

    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, CMD_OPEN_FILE,   L"Open &File...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, CMD_OPEN_FOLDER, L"Open F&older...\tCtrl+Shift+O");
    AppendMenuW(fileMenu, MF_STRING, CMD_ADD_FOLDER,  L"&Add Folder...");
    AppendMenuW(menuBar,  MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");

    // Library sort menu
    HMENU libMenu = CreatePopupMenu();
    HMENU sortSub = CreatePopupMenu();
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_TITLE,    L"By &Title");
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_ARTIST,   L"By &Artist");
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_ALBUM,    L"By A&lbum");
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_TRACKNUM, L"By Track &Number");
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_FILENAME, L"By &Filename");
    AppendMenuW(sortSub, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sortSub, MF_STRING, CMD_SORT_TOGGLE,   L"Toggle &Ascending/Descending");
    AppendMenuW(libMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(sortSub), L"&Sort");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(libMenu), L"&Library");

    HMENU viewMenu = CreatePopupMenu();
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_PANEL, L"Toggle &Side Panel");
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_NP,    L"Toggle &Now Playing");
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_BAR,   L"Toggle Progress &Bar");
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_BTNS,  L"Toggle &Transport Buttons");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_CF,    L"&Cover Flow\tF5");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_STRING, CMD_TOGGLE_VIS,   L"Toggle &Visualizer");
    AppendMenuW(viewMenu, MF_STRING, CMD_VIS_OFF,      L"Visualizer: Off");
    AppendMenuW(viewMenu, MF_STRING, CMD_VIS_SPECTRUM,  L"Visualizer: Spectrum");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_STRING, CMD_RESET_LAYOUT, L"&Reset Layout");
    AppendMenuW(viewMenu, MF_STRING, CMD_SAVE_PRESET,  L"&Save Current as Preset...");
    AppendMenuW(menuBar,  MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");

    HMENU plMenu = CreatePopupMenu();
    AppendMenuW(plMenu, MF_STRING, CMD_NEW_PLAYLIST, L"&New Playlist");
    AppendMenuW(plMenu, MF_STRING, CMD_DEL_PLAYLIST, L"&Delete Current Playlist");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(plMenu), L"&Playlist");

    // Theme menu
    HMENU themeMenu = CreatePopupMenu();

    HMENU presetSub = CreatePopupMenu();
    AppendMenuW(presetSub, MF_STRING, CMD_THEME_BASE + 0, L"Default &Dark");
    AppendMenuW(presetSub, MF_STRING, CMD_THEME_BASE + 1, L"Dark &Blue");
    AppendMenuW(presetSub, MF_STRING, CMD_THEME_BASE + 2, L"&Light");
    AppendMenuW(presetSub, MF_STRING, CMD_THEME_BASE + 3, L"&High Contrast Dark");
    AppendMenuW(themeMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(presetSub), L"&Theme Preset");

    HMENU accentSub = CreatePopupMenu();
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 0, L"Default (theme)");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 1, L"Blue");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 2, L"Purple");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 3, L"Teal");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 4, L"Green");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 5, L"Orange");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 6, L"Red");
    AppendMenuW(accentSub, MF_STRING, CMD_ACCENT_BASE + 7, L"Pink");
    AppendMenuW(themeMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(accentSub), L"&Accent Color");

    AppendMenuW(themeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(themeMenu, MF_STRING, CMD_THEME_RESET, L"&Reset to Default");

    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu), L"&Theme");

    // Plugins menu
    HMENU pluginMenu = CreatePopupMenu();
    AppendMenuW(pluginMenu, MF_STRING, CMD_PLUGIN_INFO, L"&Plugin Info...");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(pluginMenu), L"&Plugins");

    // Audio menu — built dynamically via refreshAudioMenu()
    HMENU audioMenu = CreatePopupMenu();
    AppendMenuW(audioMenu, MF_STRING | MF_GRAYED, 0, L"(loading...)");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(audioMenu), L"&Audio");

    // Help menu
    HMENU helpMenu = CreatePopupMenu();
    AppendMenuW(helpMenu, MF_STRING, CMD_ABOUT,         L"&About Wave...");
    AppendMenuW(helpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(helpMenu, MF_STRING, CMD_OPEN_LOG,      L"Open &Log File");
    AppendMenuW(helpMenu, MF_STRING, CMD_OPEN_DATA_DIR, L"Open &Data Folder");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");

    SetMenu(hwnd, menuBar);
}

bool MainWindow::create(HINSTANCE hInstance, App* app, const Settings& settings) {
    m_wndData.app        = app;
    m_wndData.engine     = &app->engine();
    m_wndData.renderer   = nullptr;
    m_wndData.library    = &app->library();
    m_wndData.mainWindow = this;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = wndProc;
    wc.hInstance      = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm       = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wc)) {
        log::error("Failed to register window class");
        return false;
    }

    // Use saved position/size or center on screen
    int x, y, w, h;
    if (settings.windowX >= 0 && settings.windowY >= 0) {
        x = settings.windowX;
        y = settings.windowY;
        w = settings.windowWidth;
        h = settings.windowHeight;
    } else {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        w = DEFAULT_WIDTH;
        h = DEFAULT_HEIGHT;
        x = (screenW - w) / 2;
        y = (screenH - h) / 2;
    }

    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Wave", WS_OVERLAPPEDWINDOW,
        x, y, w, h,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!m_hwnd) {
        log::error("Failed to create window");
        return false;
    }

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&m_wndData));
    platform::enableDarkTitleBar(m_hwnd);
    createMenu(m_hwnd);
    SetTimer(m_hwnd, TIMER_REDRAW, TIMER_INTERVAL_MS, nullptr);

    log::info("Main window created");
    return true;
}

void MainWindow::setRenderer(Renderer* r) {
    m_wndData.renderer = r;
}

void MainWindow::refreshPluginMenu() {
    if (!m_hwnd || !m_wndData.app) return;
    HMENU menuBar = GetMenu(m_hwnd);
    if (!menuBar) return;

    // Find the Plugins menu (last menu item)
    int menuCount = GetMenuItemCount(menuBar);
    HMENU pluginMenu = nullptr;
    for (int i = 0; i < menuCount; i++) {
        wchar_t buf[64]{};
        GetMenuStringW(menuBar, i, buf, 64, MF_BYPOSITION);
        if (wcsstr(buf, L"Plugins") || wcsstr(buf, L"&Plugins")) {
            pluginMenu = GetSubMenu(menuBar, i);
            break;
        }
    }
    if (!pluginMenu) return;

    // Remove old plugin command entries (keep Plugin Info at top)
    while (GetMenuItemCount(pluginMenu) > 1)
        DeleteMenu(pluginMenu, 1, MF_BYPOSITION);

    // Add separator + plugin commands
    auto& cmds = m_wndData.app->pluginHost().commands();
    if (!cmds.empty()) {
        AppendMenuW(pluginMenu, MF_SEPARATOR, 0, nullptr);
        for (int i = 0; i < static_cast<int>(cmds.size()) && i < 50; i++) {
            std::wstring name(cmds[i].name.begin(), cmds[i].name.end());
            AppendMenuW(pluginMenu, MF_STRING, CMD_PLUGIN_CMD_BASE + i, name.c_str());
        }
    }

    DrawMenuBar(m_hwnd);
}

void MainWindow::refreshAudioMenu() {
    if (!m_hwnd || !m_wndData.app) return;
    HMENU menuBar = GetMenu(m_hwnd);
    if (!menuBar) return;

    // Find the Audio menu
    int menuCount = GetMenuItemCount(menuBar);
    HMENU audioMenu = nullptr;
    for (int i = 0; i < menuCount; i++) {
        wchar_t buf[64]{};
        GetMenuStringW(menuBar, i, buf, 64, MF_BYPOSITION);
        if (wcsstr(buf, L"Audio") || wcsstr(buf, L"&Audio")) {
            audioMenu = GetSubMenu(menuBar, i);
            break;
        }
    }
    if (!audioMenu) return;

    // Clear existing items
    while (GetMenuItemCount(audioMenu) > 0)
        DeleteMenu(audioMenu, 0, MF_BYPOSITION);

    auto& settings = m_wndData.app->settings();

    // ── Output Backend ──
    HMENU aoSub = CreatePopupMenu();
    AppendMenuW(aoSub, MF_STRING | (settings.audioBackend == 0 ? MF_CHECKED : 0), CMD_AO_AUTO,   L"&Auto (default)");
    AppendMenuW(aoSub, MF_STRING | (settings.audioBackend == 1 ? MF_CHECKED : 0), CMD_AO_WASAPI, L"&WASAPI");
    AppendMenuW(aoSub, MF_STRING | (settings.audioBackend == 2 ? MF_CHECKED : 0), CMD_AO_DSOUND, L"&DirectSound");
    AppendMenuW(audioMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(aoSub), L"Output &Backend");

    // ── Output Device ──
    auto devices = m_wndData.app->engine().getAudioDevices();
    if (!devices.empty()) {
        HMENU devSub = CreatePopupMenu();
        std::string currentDev = m_wndData.app->engine().currentAudioDevice();
        int idx = 0;
        for (auto& dev : devices) {
            if (idx >= static_cast<int>(CMD_DEVICE_MAX - CMD_DEVICE_BASE)) break;
            std::wstring label(dev.name.begin(), dev.name.end());
            UINT flags = MF_STRING;
            if (dev.id == currentDev || (currentDev.empty() && dev.id == "auto"))
                flags |= MF_CHECKED;
            AppendMenuW(devSub, flags, CMD_DEVICE_BASE + idx, label.c_str());
            idx++;
        }
        AppendMenuW(audioMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(devSub), L"Output &Device");
    }

    AppendMenuW(audioMenu, MF_SEPARATOR, 0, nullptr);

    // ── Gapless ──
    AppendMenuW(audioMenu, MF_STRING | (settings.gapless ? MF_CHECKED : 0),
                CMD_GAPLESS_TOG, L"&Gapless Playback");

    AppendMenuW(audioMenu, MF_SEPARATOR, 0, nullptr);

    // ── ReplayGain ──
    HMENU rgSub = CreatePopupMenu();
    AppendMenuW(rgSub, MF_STRING | (settings.replayGain == 0 ? MF_CHECKED : 0), CMD_RG_OFF,   L"&Off");
    AppendMenuW(rgSub, MF_STRING | (settings.replayGain == 1 ? MF_CHECKED : 0), CMD_RG_TRACK, L"&Track");
    AppendMenuW(rgSub, MF_STRING | (settings.replayGain == 2 ? MF_CHECKED : 0), CMD_RG_ALBUM, L"&Album");
    AppendMenuW(rgSub, MF_SEPARATOR, 0, nullptr);
    wchar_t preampLabel[64];
    swprintf_s(preampLabel, 64, L"Preamp: %+.0f dB", settings.replayGainPreamp);
    AppendMenuW(rgSub, MF_STRING | MF_GRAYED, 0, preampLabel);
    AppendMenuW(rgSub, MF_STRING, CMD_RG_PREAMP_UP, L"Preamp &+3 dB");
    AppendMenuW(rgSub, MF_STRING, CMD_RG_PREAMP_DN, L"Preamp &-3 dB");
    AppendMenuW(audioMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(rgSub), L"&ReplayGain");

    AppendMenuW(audioMenu, MF_SEPARATOR, 0, nullptr);

    // ── Start Paused ──
    AppendMenuW(audioMenu, MF_STRING | (settings.startPaused ? MF_CHECKED : 0),
                CMD_START_PAUSED, L"&Start Paused");

    AppendMenuW(audioMenu, MF_SEPARATOR, 0, nullptr);

    // ── Shuffle + Repeat ──
    AppendMenuW(audioMenu, MF_STRING | (settings.shuffle ? MF_CHECKED : 0),
                CMD_SHUFFLE_TOG, L"Sh&uffle");

    const wchar_t* repeatLabels[] = { L"Repeat: &Off", L"Repeat: &All", L"Repeat: &One" };
    int rmi = std::clamp(settings.repeatMode, 0, 2);
    AppendMenuW(audioMenu, MF_STRING | (rmi > 0 ? MF_CHECKED : 0),
                CMD_REPEAT_CYCLE, repeatLabels[rmi]);

    AppendMenuW(audioMenu, MF_SEPARATOR, 0, nullptr);

    // ── Equalizer ──
    HMENU eqSub = CreatePopupMenu();
    auto& eq = m_wndData.app->equalizer();
    AppendMenuW(eqSub, MF_STRING | (eq.enabled() ? MF_CHECKED : 0),
                CMD_EQ_TOGGLE, L"&Enable EQ");
    AppendMenuW(eqSub, MF_SEPARATOR, 0, nullptr);

    for (int i = 0; i < Equalizer::presetCount(); i++) {
        auto& p = Equalizer::preset(i);
        UINT flags = MF_STRING;
        if (eq.enabled() && eq.currentPreset() == i) flags |= MF_CHECKED;
        AppendMenuW(eqSub, flags, CMD_EQ_PRESET_BASE + i, p.name);
    }

    AppendMenuW(audioMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(eqSub), L"&Equalizer");

    DrawMenuBar(m_hwnd);
}

void MainWindow::show(const Settings& /*settings*/) {
    ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(m_hwnd);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
}

void MainWindow::saveWindowState(Settings& settings) const {
    if (!m_hwnd) return;

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    GetWindowPlacement(m_hwnd, &wp);

    settings.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

    // Save the normal (non-maximized) position so we restore correctly
    RECT r = wp.rcNormalPosition;
    settings.windowX      = r.left;
    settings.windowY      = r.top;
    settings.windowWidth  = r.right - r.left;
    settings.windowHeight = r.bottom - r.top;
}

// ── Helper: get the TrackInfo for a panel row based on active tab ──

static const TrackInfo* getTrackForRow(Renderer* renderer, Library* library,
                                        PlaylistManager* plMgr, int row) {
    if (!renderer || row < 0) return nullptr;
    switch (renderer->activeTab()) {
        case PanelTab::Library:
            return library ? library->trackAt(row) : nullptr;
        case PanelTab::Playlists: {
            int viewPl = renderer->viewedPlaylist();
            if (viewPl < 0) return nullptr; // playlist list, not tracks
            auto* pl = plMgr ? plMgr->playlist(viewPl) : nullptr;
            if (!pl || row < 0 || row >= static_cast<int>(pl->tracks.size())) return nullptr;
            return &pl->tracks[row];
        }
        case PanelTab::Queue:
            if (!plMgr || row < 0 || row >= plMgr->queueCount()) return nullptr;
            return &plMgr->queue()[row];
    }
    return nullptr;
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<WndData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) return DefWindowProcW(hwnd, msg, wp, lp);

    App*             app      = data->app;
    Engine*          engine   = data->engine;
    Renderer*        renderer = data->renderer;
    Library*         library  = data->library;
    PlaylistManager* plMgr    = app ? &app->playlistMgr() : nullptr;
    Layout*          layout   = app ? &app->layout() : nullptr;
    Theme*           theme    = app ? &app->theme()  : nullptr;

    switch (msg) {
        case WM_INITMENUPOPUP: {
            // Only refresh Audio menu when it's actually being opened
            HMENU popup = reinterpret_cast<HMENU>(wp);
            HMENU menuBar = GetMenu(hwnd);
            if (menuBar && data->mainWindow) {
                int cnt = GetMenuItemCount(menuBar);
                for (int i = 0; i < cnt; i++) {
                    if (GetSubMenu(menuBar, i) == popup) {
                        wchar_t buf[32]{};
                        GetMenuStringW(menuBar, i, buf, 32, MF_BYPOSITION);
                        if (wcsstr(buf, L"Audio"))
                            data->mainWindow->refreshAudioMenu();
                        break;
                    }
                }
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_REDRAW);
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
            ValidateRect(hwnd, nullptr);
            if (renderer) renderer->render();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (renderer && wp != SIZE_MINIMIZED) renderer->resize();
            return 0;

        case WM_TIMER:
            if (wp == TIMER_REDRAW && renderer) {
                if (app) renderer->setShuffleRepeat(app->shuffle(), static_cast<int>(app->repeatMode()));
                renderer->render();
            }
            return 0;

        case Engine::WM_MPV_WAKEUP:
            if (engine) engine->processEvents();
            return 0;

        // ── Menu commands ────────────────────────────────────
        case WM_COMMAND: {
            UINT cmd = LOWORD(wp);
            switch (cmd) {
                case CMD_OPEN_FILE:    if (app) app->openFile();       return 0;
                case CMD_OPEN_FOLDER:  if (app) app->openFolder();     return 0;
                case CMD_ADD_FOLDER:   if (app) app->addFolderToLibrary(); return 0;
                case CMD_NEW_PLAYLIST: if (app) app->createPlaylist(); return 0;
                case CMD_TOGGLE_PANEL: if (layout) layout->togglePanel(PanelId::SidePanel);     return 0;
                case CMD_TOGGLE_NP:    if (layout) layout->togglePanel(PanelId::NowPlaying);    return 0;
                case CMD_TOGGLE_BAR:   if (layout) layout->togglePanel(PanelId::TransportBar);  return 0;
                case CMD_TOGGLE_BTNS:  if (layout) layout->togglePanel(PanelId::TransportBtns); return 0;
                case CMD_RESET_LAYOUT: if (layout) layout->resetToDefault();                    return 0;
                case CMD_VIS_OFF:
                    if (app) app->visualizer().setMode(VisMode::Off);
                    return 0;
                case CMD_VIS_SPECTRUM:
                    if (app) app->visualizer().setMode(VisMode::Spectrum);
                    return 0;
                case CMD_TOGGLE_CF:
                    if (renderer) {
                        renderer->setCoverFlowMode(!renderer->isCoverFlowMode());
                    }
                    return 0;
                case CMD_TOGGLE_VIS:
                    if (app) {
                        auto& vis = app->visualizer();
                        vis.setMode(vis.mode() == VisMode::Off ? VisMode::Spectrum : VisMode::Off);
                    }
                    return 0;
                case CMD_SAVE_PRESET:
                    if (layout) {
                        int n = static_cast<int>(layout->presets().size()) + 1;
                        layout->saveCurrentAsPreset(L"Preset " + std::to_wstring(n));
                    }
                    return 0;
                case CMD_DEL_PLAYLIST:
                    if (app && renderer && renderer->activeTab() == PanelTab::Playlists) {
                        int vp = renderer->viewedPlaylist();
                        if (vp >= 0) {
                            app->deletePlaylist(vp);
                            renderer->setViewedPlaylist(-1);
                        }
                    }
                    return 0;
            }
            // Context menu: add to playlist N
            // Theme preset selection
            if (cmd >= CMD_THEME_BASE && cmd < CMD_THEME_BASE + 20) {
                if (theme && renderer) {
                    theme->applyPreset(cmd - CMD_THEME_BASE);
                    renderer->applyTheme();
                }
                return 0;
            }
            // Accent color selection
            if (cmd >= CMD_ACCENT_BASE && cmd < CMD_ACCENT_BASE + 20) {
                if (theme && renderer) {
                    theme->setAccent(cmd - CMD_ACCENT_BASE);
                    renderer->applyTheme();
                }
                return 0;
            }
            if (cmd == CMD_THEME_RESET) {
                if (theme && renderer) {
                    theme->resetToDefault();
                    renderer->applyTheme();
                }
                return 0;
            }
            if (cmd == CMD_ABOUT) {
                MessageBoxW(hwnd,
                    L"Wave Audio Player\n"
                    L"Version 0.1.0-beta\n\n"
                    L"A lightweight, high-performance local music player\n"
                    L"for Windows, built with C++, Direct2D, and libmpv.\n\n"
                    L"Features:\n"
                    L"  \x2022  Local audio playback (MP3, FLAC, WAV, AAC, OGG, Opus, ...)\n"
                    L"  \x2022  Library browsing with search and sort\n"
                    L"  \x2022  Playlists and queue management\n"
                    L"  \x2022  Album art and metadata display\n"
                    L"  \x2022  Real-time spectrum visualizer\n"
                    L"  \x2022  Themes and layout customization\n"
                    L"  \x2022  Native plugin SDK\n\n"
                    L"Keyboard shortcuts:\n"
                    L"  Space = Play/Pause  |  \x2190\x2192 = Seek  |  \x2191\x2193 = Volume\n"
                    L"  Ctrl+O = Open File  |  Ctrl+Shift+O = Open Folder\n"
                    L"  Ctrl+F = Search  |  Escape = Clear search\n\n"
                    L"\xa9 2026  |  Built with libmpv, Direct2D, DirectWrite",
                    L"About Wave", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (cmd == CMD_OPEN_LOG) {
                std::wstring logPath = Settings::dataDir() + L"\\wave.log";
                ShellExecuteW(hwnd, L"open", L"notepad.exe", logPath.c_str(), nullptr, SW_SHOW);
                return 0;
            }
            if (cmd == CMD_OPEN_DATA_DIR) {
                std::wstring dir = Settings::dataDir();
                ShellExecuteW(hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOW);
                return 0;
            }
            if (cmd == CMD_PLUGIN_INFO) {
                if (app) {
                    auto& plugins = app->pluginHost().plugins();
                    std::wstring infoMsg;
                    if (plugins.empty()) {
                        infoMsg = L"No plugins found.\n\nPlace .dll plugins in:\nbuild\\Debug\\plugins\\";
                    } else {
                        infoMsg = L"Loaded plugins:\n\n";
                        for (auto& p : plugins) {
                            std::wstring pname(p.name.begin(), p.name.end());
                            std::wstring ver(p.version.begin(), p.version.end());
                            const wchar_t* pstate = L"?";
                            switch (p.state) {
                                case WAVE_PLUGIN_ACTIVE:   pstate = L"Active"; break;
                                case WAVE_PLUGIN_ERROR:    pstate = L"Error"; break;
                                case WAVE_PLUGIN_LOADED:   pstate = L"Loaded"; break;
                                case WAVE_PLUGIN_UNLOADED: pstate = L"Unloaded"; break;
                                default: break;
                            }
                            infoMsg += pname + L" v" + ver + L"  [" + pstate + L"]\n";
                        }
                    }
                    MessageBoxW(hwnd, infoMsg.c_str(), L"Wave Plugins", MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            // Layout preset selection
            if (cmd >= CMD_PRESET_BASE && cmd < CMD_PRESET_BASE + 20) {
                if (layout) layout->applyPreset(cmd - CMD_PRESET_BASE);
                return 0;
            }
            // Audio backend (requires restart)
            if (cmd == CMD_AO_AUTO || cmd == CMD_AO_WASAPI || cmd == CMD_AO_DSOUND) {
                if (app) {
                    int val = (cmd == CMD_AO_WASAPI) ? 1 : (cmd == CMD_AO_DSOUND) ? 2 : 0;
                    app->settings().audioBackend = val;
                    const wchar_t* names[] = { L"Auto", L"WASAPI", L"DirectSound" };
                    std::wstring amsg = L"Audio backend set to: ";
                    amsg += names[val];
                    amsg += L"\n\nRestart Wave for changes to take effect.";
                    MessageBoxW(hwnd, amsg.c_str(), L"Audio Settings", MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            // Audio device selection (live)
            if (cmd >= CMD_DEVICE_BASE && cmd < CMD_DEVICE_MAX && app) {
                auto devices = app->engine().getAudioDevices();
                int devIdx = cmd - CMD_DEVICE_BASE;
                if (devIdx >= 0 && devIdx < static_cast<int>(devices.size())) {
                    app->engine().setAudioDevice(devices[devIdx].id);
                    app->settings().audioDevice = devices[devIdx].id;
                }
                return 0;
            }
            // Gapless (live)
            if (cmd == CMD_GAPLESS_TOG && app) {
                app->settings().gapless = !app->settings().gapless;
                app->engine().setGapless(app->settings().gapless);
                return 0;
            }
            // ReplayGain mode (live)
            if (cmd >= CMD_RG_OFF && cmd <= CMD_RG_ALBUM && app) {
                app->settings().replayGain = cmd - CMD_RG_OFF;
                app->engine().setReplayGain(static_cast<ReplayGainMode>(cmd - CMD_RG_OFF));
                return 0;
            }
            // ReplayGain preamp +/- 3dB
            if (cmd == CMD_RG_PREAMP_UP && app) {
                app->settings().replayGainPreamp = std::clamp(app->settings().replayGainPreamp + 3.0, -15.0, 15.0);
                app->engine().setReplayGainPreamp(app->settings().replayGainPreamp);
                return 0;
            }
            if (cmd == CMD_RG_PREAMP_DN && app) {
                app->settings().replayGainPreamp = std::clamp(app->settings().replayGainPreamp - 3.0, -15.0, 15.0);
                app->engine().setReplayGainPreamp(app->settings().replayGainPreamp);
                return 0;
            }
            // Start paused toggle
            if (cmd == CMD_START_PAUSED && app) {
                app->settings().startPaused = !app->settings().startPaused;
                return 0;
            }
            // Shuffle toggle
            if (cmd == CMD_SHUFFLE_TOG && app) {
                app->toggleShuffle();
                return 0;
            }
            // Repeat cycle
            if (cmd == CMD_REPEAT_CYCLE && app) {
                app->cycleRepeat();
                return 0;
            }
            // EQ toggle
            if (cmd == CMD_EQ_TOGGLE && app) {
                auto& eq = app->equalizer();
                eq.setEnabled(!eq.enabled());
                eq.applyToEngine(&app->engine());
                return 0;
            }
            // EQ presets
            if (cmd >= CMD_EQ_PRESET_BASE && cmd < CMD_EQ_PRESET_BASE + 20 && app) {
                int presetIdx = cmd - CMD_EQ_PRESET_BASE;
                auto& eq = app->equalizer();
                eq.applyPreset(presetIdx);
                if (!eq.enabled()) eq.setEnabled(true);
                eq.applyToEngine(&app->engine());
                return 0;
            }
            // Sort commands
            if (cmd >= CMD_SORT_TITLE && cmd <= CMD_SORT_FILENAME && library) {
                SortField sf = SortField::Title;
                switch (cmd) {
                    case CMD_SORT_TITLE:    sf = SortField::Title; break;
                    case CMD_SORT_ARTIST:   sf = SortField::Artist; break;
                    case CMD_SORT_ALBUM:    sf = SortField::Album; break;
                    case CMD_SORT_TRACKNUM: sf = SortField::TrackNumber; break;
                    case CMD_SORT_FILENAME: sf = SortField::FileName; break;
                }
                library->setSort(sf, library->sortAscending());
                if (renderer) renderer->setScrollOffset(0);
                return 0;
            }
            if (cmd == CMD_SORT_TOGGLE && library) {
                library->setSort(library->sortField(), !library->sortAscending());
                if (renderer) renderer->setScrollOffset(0);
                return 0;
            }
            // Plugin commands
            if (cmd >= CMD_PLUGIN_CMD_BASE && cmd < CMD_PLUGIN_CMD_BASE + 50) {
                if (app) {
                    auto& cmds = app->pluginHost().commands();
                    int idx = cmd - CMD_PLUGIN_CMD_BASE;
                    if (idx >= 0 && idx < static_cast<int>(cmds.size()))
                        app->pluginHost().executeCommand(cmds[idx].id);
                }
                return 0;
            }
            if (cmd >= CMD_CTX_PL_BASE && cmd < CMD_CTX_PL_BASE + 100) {
                int plIdx = cmd - CMD_CTX_PL_BASE;
                auto* track = getTrackForRow(renderer, library, plMgr, data->contextRow);
                if (track && app) app->addTrackToPlaylist(plIdx, *track);
                return 0;
            }
            if (cmd == CMD_CTX_PLAY) {
                if (app && data->contextRow >= 0) {
                    if (renderer && renderer->activeTab() == PanelTab::Library)
                        app->playTrack(data->contextRow);
                    else if (renderer && renderer->activeTab() == PanelTab::Playlists)
                        app->playPlaylistTrack(renderer->viewedPlaylist(), data->contextRow);
                }
                return 0;
            }
            if (cmd == CMD_CTX_QUEUE) {
                auto* track = getTrackForRow(renderer, library, plMgr, data->contextRow);
                if (track && app) app->addTrackToQueue(*track);
                return 0;
            }
            break;
        }

        // ── Mouse input ──────────────────────────────────────
        case WM_MOUSEMOVE: {
            if (!renderer) break;
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));

            TRACKMOUSEEVENT tme{}; tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE; tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            HitZone zone = renderer->hitTest(mx, my);
            renderer->setHover(zone);

            bool clickable = (zone != HitZone::None);
            SetCursor(LoadCursor(nullptr, clickable ? IDC_HAND : IDC_ARROW));

            if ((wp & MK_LBUTTON) && data->draggingBar && engine) {
                renderer->updateScrub(mx, my);
                // Don't seek live during drag — causes stutter.
                // Visual position updates via scrubFrac. Seek on release only.
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (renderer) renderer->setHover(HitZone::None);
            return 0;

        case WM_LBUTTONDOWN: {
            if (!renderer) break;
            SetFocus(hwnd);
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));

            // Cover Flow click handling
            if (renderer->isCoverFlowMode() && app) {
                int cfAlbum = renderer->coverFlowHitTest(mx, my);
                if (cfAlbum >= 0) {
                    int focused = app->coverFlow().focusedIndex();
                    if (cfAlbum == focused) {
                        // Click center album → play it
                        app->playCoverFlowAlbum();
                    } else {
                        // Click side album → smoothly scroll to it
                        app->coverFlow().scrollBy(cfAlbum - focused);
                    }
                    return 0;
                }
            }

            HitZone zone = renderer->hitTest(mx, my);
            renderer->setPressed(zone);

            if (zone == HitZone::ProgressBar) {
                data->draggingBar = true;
                SetCapture(hwnd);
                renderer->beginScrub(my);
                // Set initial visual position but don't seek yet — seek on release
                renderer->updateScrub(mx, my);
            }

            // Tab clicks
            if (zone == HitZone::TabLibrary)   renderer->setActiveTab(PanelTab::Library);
            if (zone == HitZone::TabPlaylists) { renderer->setActiveTab(PanelTab::Playlists); renderer->setViewedPlaylist(-1); }
            if (zone == HitZone::TabQueue)     renderer->setActiveTab(PanelTab::Queue);

            // Panel row single-click
            if (zone == HitZone::PanelRow) {
                int row = renderer->panelRowAt(my);
                if (row < 0) { /* nothing */ }
                else if (renderer->activeTab() == PanelTab::Library) {
                    if (app) app->playTrack(row);
                }
                else if (renderer->activeTab() == PanelTab::Playlists) {
                    if (renderer->viewedPlaylist() < 0) {
                        if (plMgr && row >= 0 && row < plMgr->playlistCount())
                            renderer->setViewedPlaylist(row);
                    } else if (row == 0) {
                        renderer->setViewedPlaylist(-1);
                    } else {
                        int trackIdx = row - 1;
                        if (app && trackIdx >= 0)
                            app->playPlaylistTrack(renderer->viewedPlaylist(), trackIdx);
                    }
                }
                else if (renderer->activeTab() == PanelTab::Queue) {
                    if (plMgr && row >= 0 && row < plMgr->queueCount() && engine) {
                        const auto& track = plMgr->queue()[row];
                        std::string path = platform::toUtf8(track.fullPath);
                        engine->loadFile(path);
                    }
                }
            }

            return 0;
        }

        case WM_LBUTTONUP: {
            if (!renderer) break;
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));
            HitZone zone = renderer->hitTest(mx, my);
            HitZone wasPressed = renderer->pressed();
            renderer->setPressed(HitZone::None);

            if (data->draggingBar) {
                bool wasPreview = renderer->isScrubPreview();
                float previewFrac = renderer->scrubFraction();
                renderer->endScrub();
                data->draggingBar = false;
                ReleaseCapture();
                if (engine) {
                    // In preview mode: seek to the preview position
                    // In normal mode: seek to current mouse position
                    float frac = wasPreview ? previewFrac : renderer->progressBarFraction(mx);
                    double dur = engine->duration();
                    if (dur > 0) engine->seekAbsolute(frac * dur);
                }
            }

            if (zone == wasPressed) {
                switch (zone) {
                    case HitZone::PlayPause:
                        if (engine && engine->state() != PlaybackState::Stopped) {
                            engine->togglePause();
                        } else if (app && library && !library->empty()) {
                            int idx = library->playingViewIndex();
                            if (idx < 0) idx = 0;
                            app->playTrack(idx);
                        }
                        break;
                    case HitZone::Stop:      if (engine) engine->stop();        break;
                    case HitZone::Prev:      if (app) app->playPrev();          break;
                    case HitZone::Next:      if (app) app->playNext();          break;
                    default: break;
                }
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK:
            // Handled by single-click in WM_LBUTTONDOWN
            return 0;

        case WM_RBUTTONUP: {
            if (!renderer || !app) break;
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));
            HitZone zone = renderer->hitTest(mx, my);

            if (zone == HitZone::PanelRow) {
                int row = renderer->panelRowAt(my);
                // Adjust for playlist header
                if (renderer->activeTab() == PanelTab::Playlists && renderer->viewedPlaylist() >= 0)
                    row -= 1;

                auto* track = getTrackForRow(renderer, library, plMgr, row);
                if (!track) break;
                data->contextRow = row;

                HMENU popup = CreatePopupMenu();
                AppendMenuW(popup, MF_STRING, CMD_CTX_PLAY,  L"Play");
                AppendMenuW(popup, MF_STRING, CMD_CTX_QUEUE, L"Add to Queue");

                if (plMgr && plMgr->playlistCount() > 0) {
                    HMENU plSub = CreatePopupMenu();
                    for (int i = 0; i < plMgr->playlistCount(); i++) {
                        AppendMenuW(plSub, MF_STRING, CMD_CTX_PL_BASE + i,
                                     plMgr->playlists()[i].name.c_str());
                    }
                    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(plSub),
                                 L"Add to Playlist");
                }

                POINT pt = { static_cast<LONG>(mx), static_cast<LONG>(my) };
                ClientToScreen(hwnd, &pt);
                TrackPopupMenu(popup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(popup);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (!renderer) break;
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            HitZone zone = renderer->hitTest(static_cast<float>(pt.x), static_cast<float>(pt.y));

            if (zone == HitZone::ProgressBar && engine) {
                engine->seekRelative(delta > 0 ? 1.0 : -1.0);
            } else if (renderer->isCoverFlowMode() && app &&
                       zone != HitZone::PanelRow && zone != HitZone::TabLibrary &&
                       zone != HitZone::TabPlaylists && zone != HitZone::TabQueue) {
                // Scroll cover flow
                app->coverFlow().scrollBy(delta > 0 ? -1 : 1);
            } else {
                renderer->scrollPanel(delta > 0 ? -3 : 3);
            }
            return 0;
        }

        // ── Search typing (WM_CHAR) ─────────────────────────
        case WM_CHAR: {
            if (!library || !renderer) break;
            if (renderer->activeTab() != PanelTab::Library) break;

            wchar_t ch = static_cast<wchar_t>(wp);
            if (ch >= 32) { // printable character
                std::wstring q = library->searchQuery();
                q += ch;
                library->setSearch(q);
                renderer->setScrollOffset(0);
                return 0;
            }
            if (ch == 8) { // backspace
                std::wstring q = library->searchQuery();
                if (!q.empty()) {
                    q.pop_back();
                    library->setSearch(q);
                    renderer->setScrollOffset(0);
                }
                return 0;
            }
            break;
        }

        // ── Keyboard ─────────────────────────────────────────
        case WM_KEYDOWN: {
            // F5 toggles Cover Flow (works even without engine)
            if (wp == VK_F5 && renderer) {
                renderer->setCoverFlowMode(!renderer->isCoverFlowMode());
                return 0;
            }

            // Cover Flow mode navigation (before engine check)
            if (renderer && renderer->isCoverFlowMode() && app) {
                switch (wp) {
                    case VK_LEFT:
                        app->coverFlow().moveLeft();
                        return 0;
                    case VK_RIGHT:
                        app->coverFlow().moveRight();
                        return 0;
                    case VK_RETURN:
                        app->playCoverFlowAlbum();
                        return 0;
                    case VK_ESCAPE:
                        renderer->setCoverFlowMode(false);
                        return 0;
                }
            }

            if (!engine) break;

            // Escape clears search
            if (wp == VK_ESCAPE && library && renderer &&
                renderer->activeTab() == PanelTab::Library &&
                !library->searchQuery().empty()) {
                library->setSearch(L"");
                renderer->setScrollOffset(0);
                return 0;
            }

            // Ctrl+F focuses search (clears existing)
            if (wp == 'F' && (GetKeyState(VK_CONTROL) & 0x8000) && library && renderer) {
                renderer->setActiveTab(PanelTab::Library);
                library->setSearch(L"");
                renderer->setScrollOffset(0);
                return 0;
            }

            switch (wp) {
                case VK_SPACE:
                    if (engine->state() != PlaybackState::Stopped) {
                        engine->togglePause();
                    } else if (app && library && !library->empty()) {
                        int idx = library->playingViewIndex();
                        if (idx < 0) idx = 0;
                        app->playTrack(idx);
                    }
                    return 0;
                case VK_LEFT:   engine->seekRelative(-5.0);  return 0;
                case VK_RIGHT:  engine->seekRelative(5.0);   return 0;
                case VK_UP:     engine->setVolume(engine->volume() + 1.0);  return 0;
                case VK_DOWN:   engine->setVolume(engine->volume() - 1.0);  return 0;
                case 'O':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        if (GetKeyState(VK_SHIFT) & 0x8000)
                            { if (app) app->openFolder(); }
                        else
                            { if (app) app->openFile(); }
                        return 0;
                    }
                    break;
                case VK_RETURN:
                    if (app && library && !library->empty() &&
                        renderer && renderer->activeTab() == PanelTab::Library) {
                        app->playTrack(library->selectedIndex());
                    }
                    return 0;
            }
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace wave
