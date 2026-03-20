#pragma once

#include <Windows.h>
#include <string>
#include <string_view>

namespace wave::platform {

// Register a Win32 window class. Returns true on success.
bool registerWindowClass(
    HINSTANCE hInstance,
    const wchar_t* className,
    WNDPROC wndProc,
    HBRUSH backgroundBrush = nullptr
);

// Create a centered, resizable window. Returns the HWND or nullptr on failure.
HWND createWindow(
    HINSTANCE hInstance,
    const wchar_t* className,
    const wchar_t* title,
    int width,
    int height
);

// Enable dark title bar on Windows 11 (DWM attribute).
void enableDarkTitleBar(HWND hwnd);

// Standard Win32 message loop. Returns the exit code.
int runMessageLoop();

// Convert UTF-8 to wide string.
std::wstring toWide(std::string_view utf8);

// Convert wide string to UTF-8.
std::string toUtf8(std::wstring_view wide);

} // namespace wave::platform
