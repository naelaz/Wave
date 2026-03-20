#include "platform/Win32Helpers.h"
#include "core/Log.h"

#include <dwmapi.h>

namespace wave::platform {

bool registerWindowClass(
    HINSTANCE hInstance,
    const wchar_t* className,
    WNDPROC wndProc,
    HBRUSH backgroundBrush
) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wndProc;
    wc.hInstance      = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = backgroundBrush;
    wc.lpszClassName = className;

    if (!RegisterClassExW(&wc)) {
        log::error("Failed to register window class");
        return false;
    }
    return true;
}

HWND createWindow(
    HINSTANCE hInstance,
    const wchar_t* className,
    const wchar_t* title,
    int width,
    int height
) {
    // Center on the primary monitor
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        className,
        title,
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        log::error("Failed to create window");
    }

    return hwnd;
}

void enableDarkTitleBar(HWND hwnd) {
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11+)
    BOOL useDark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
}

int runMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()), result.data(), len);
    return result;
}

std::string toUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()), result.data(), len, nullptr, nullptr);
    return result;
}

} // namespace wave::platform
