#include "window/MainWindow.h"
#include "platform/Win32Helpers.h"
#include "core/Log.h"

namespace wave {

bool MainWindow::create(HINSTANCE hInstance) {
    s_bgBrush = CreateSolidBrush(RGB(18, 18, 18));

    if (!platform::registerWindowClass(hInstance, CLASS_NAME, wndProc, s_bgBrush)) {
        return false;
    }

    m_hwnd = platform::createWindow(
        hInstance, CLASS_NAME, L"Wave",
        DEFAULT_WIDTH, DEFAULT_HEIGHT
    );

    if (!m_hwnd) {
        return false;
    }

    platform::enableDarkTitleBar(m_hwnd);
    log::info("Main window created");
    return true;
}

void MainWindow::show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, s_bgBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            // WM_PAINT fills the background — skip erase to avoid flicker
            return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace wave
