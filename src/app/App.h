#pragma once

#include <Windows.h>
#include "window/MainWindow.h"

namespace wave {

class App {
public:
    bool init(HINSTANCE hInstance);
    int run();
    void shutdown();

private:
    HINSTANCE m_hInstance = nullptr;
    MainWindow m_mainWindow;
};

} // namespace wave
