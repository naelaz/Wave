#include "app/App.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <shellapi.h>

namespace wave {

bool App::init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    log::init();
    log::info("Wave v0.1.0 starting");

    if (!m_mainWindow.create(hInstance, &m_engine)) {
        log::error("Failed to initialize main window");
        return false;
    }

    if (!m_engine.init(m_mainWindow.handle())) {
        log::warn("Audio engine not available (is libmpv-2.dll next to Wave.exe?)");
    }

    // Load file from command-line argument if provided
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1) {
            std::string path = platform::toUtf8(argv[1]);
            m_engine.loadFile(path);
        }
        LocalFree(argv);
    }

    log::info("Initialization complete");
    return true;
}

int App::run() {
    m_mainWindow.show();
    return platform::runMessageLoop();
}

void App::shutdown() {
    m_engine.shutdown();
    log::info("Wave shutting down");
    log::shutdown();
}

} // namespace wave
