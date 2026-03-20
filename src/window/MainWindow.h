#pragma once

#include <Windows.h>

namespace wave {

class MainWindow {
public:
    bool create(HINSTANCE hInstance);
    void show();
    HWND handle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    static constexpr const wchar_t* CLASS_NAME = L"WaveMainWindow";
    static constexpr int DEFAULT_WIDTH  = 1100;
    static constexpr int DEFAULT_HEIGHT = 700;

    static inline HBRUSH s_bgBrush = nullptr;

    HWND m_hwnd = nullptr;
};

} // namespace wave
