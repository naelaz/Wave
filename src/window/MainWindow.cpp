#include "window/MainWindow.h"
#include "platform/Win32Helpers.h"
#include "core/Log.h"
#include "app/App.h"
#include "audio/Engine.h"
#include "ui/Renderer.h"
#include "library/Library.h"

#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM, GET_WHEEL_DELTA_WPARAM
#include "resources/resource.h"

namespace wave {

void MainWindow::createMenu(HWND hwnd) {
    HMENU menuBar  = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, CMD_OPEN_FILE,   L"Open &File...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, CMD_OPEN_FOLDER, L"Open F&older...\tCtrl+Shift+O");
    AppendMenuW(menuBar,  MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    SetMenu(hwnd, menuBar);
}

bool MainWindow::create(HINSTANCE hInstance, App* app) {
    m_wndData.app      = app;
    m_wndData.engine   = &app->engine();
    m_wndData.renderer = nullptr; // set after renderer init via GWLP_USERDATA refresh
    m_wndData.library  = &app->library();

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

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Wave", WS_OVERLAPPEDWINDOW,
        (screenW - DEFAULT_WIDTH) / 2, (screenH - DEFAULT_HEIGHT) / 2,
        DEFAULT_WIDTH, DEFAULT_HEIGHT,
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

void MainWindow::show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<WndData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data) return DefWindowProcW(hwnd, msg, wp, lp);

    App*      app      = data->app;
    Engine*   engine   = data->engine;
    Renderer* renderer = data->renderer;
    Library*  library  = data->library;

    switch (msg) {
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
            if (renderer && wp != SIZE_MINIMIZED) {
                renderer->resize();
            }
            return 0;

        case WM_TIMER:
            if (wp == TIMER_REDRAW) {
                if (renderer) renderer->render();
            }
            return 0;

        case Engine::WM_MPV_WAKEUP:
            if (engine) engine->processEvents();
            return 0;

        // ── Menu commands ────────────────────────────────────
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case CMD_OPEN_FILE:   if (app) app->openFile();   return 0;
                case CMD_OPEN_FOLDER: if (app) app->openFolder(); return 0;
            }
            break;

        // ── Mouse input ──────────────────────────────────────
        case WM_MOUSEMOVE: {
            if (!renderer) break;
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));

            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            HitZone zone = renderer->hitTest(mx, my);
            renderer->setHover(zone);

            bool clickable = (zone == HitZone::ProgressBar || zone == HitZone::PlayPause ||
                              zone == HitZone::Stop || zone == HitZone::Prev ||
                              zone == HitZone::Next || zone == HitZone::LibraryRow);
            SetCursor(LoadCursor(nullptr, clickable ? IDC_HAND : IDC_ARROW));

            if ((wp & MK_LBUTTON) && data->draggingBar && engine) {
                float frac = renderer->progressBarFraction(mx);
                double dur = engine->duration();
                if (dur > 0.0) engine->seekAbsolute(frac * dur);
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
            HitZone zone = renderer->hitTest(mx, my);
            renderer->setPressed(zone);

            if (zone == HitZone::ProgressBar) {
                data->draggingBar = true;
                SetCapture(hwnd);
                if (engine) {
                    float frac = renderer->progressBarFraction(mx);
                    double dur = engine->duration();
                    if (dur > 0.0) engine->seekAbsolute(frac * dur);
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
                data->draggingBar = false;
                ReleaseCapture();
                if (engine) {
                    float frac = renderer->progressBarFraction(mx);
                    double dur = engine->duration();
                    if (dur > 0.0) engine->seekAbsolute(frac * dur);
                }
            }

            if (zone == wasPressed) {
                switch (zone) {
                    case HitZone::PlayPause: if (engine) engine->togglePause(); break;
                    case HitZone::Stop:      if (engine) engine->stop();        break;
                    case HitZone::Prev:      if (app) app->playPrev();          break;
                    case HitZone::Next:      if (app) app->playNext();          break;
                    default: break;
                }
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            if (!renderer || !app) break;
            float mx = static_cast<float>(GET_X_LPARAM(lp));
            float my = static_cast<float>(GET_Y_LPARAM(lp));
            HitZone zone = renderer->hitTest(mx, my);
            if (zone == HitZone::LibraryRow) {
                int row = renderer->libraryRowAt(my);
                if (row >= 0) app->playTrack(row);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (!renderer) break;
            int delta = GET_WHEEL_DELTA_WPARAM(wp);

            // WM_MOUSEWHEEL coords are screen-relative, convert to client
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            HitZone zone = renderer->hitTest(static_cast<float>(pt.x), static_cast<float>(pt.y));

            if (zone == HitZone::ProgressBar && engine) {
                // Scroll over progress bar = seek 1 second per tick
                engine->seekRelative(delta > 0 ? 1.0 : -1.0);
            } else {
                renderer->scrollLibrary(delta > 0 ? -3 : 3);
            }
            return 0;
        }

        // ── Keyboard input ───────────────────────────────────
        case WM_KEYDOWN:
            if (!engine) break;
            switch (wp) {
                case VK_SPACE:  engine->togglePause();  return 0;
                case 'S':       engine->stop();         return 0;
                case VK_LEFT:   engine->seekRelative(-5.0);  return 0;
                case VK_RIGHT:  engine->seekRelative(5.0);   return 0;
                case VK_UP:     engine->setVolume(engine->volume() + 5.0);  return 0;
                case VK_DOWN:   engine->setVolume(engine->volume() - 5.0);  return 0;
                case 'O':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        if (GetKeyState(VK_SHIFT) & 0x8000) {
                            if (app) app->openFolder();
                        } else {
                            if (app) app->openFile();
                        }
                        return 0;
                    }
                    break;
                case VK_RETURN:
                    if (app && library && !library->empty()) {
                        app->playTrack(library->selectedIndex());
                    }
                    return 0;
            }
            break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace wave
