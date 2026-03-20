#include "app/App.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

namespace wave {

bool App::init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    log::init();
    log::info("Wave v0.1.0 starting");

    if (!m_mainWindow.create(hInstance)) {
        log::error("Failed to initialize main window");
        return false;
    }

    // Future init points:
    // - Audio engine (libmpv)
    // - Library manager
    // - Settings loader
    // - Plugin host

    log::info("Initialization complete");
    return true;
}

int App::run() {
    m_mainWindow.show();
    return platform::runMessageLoop();
}

void App::shutdown() {
    // Future shutdown points:
    // - Save settings
    // - Release audio engine
    // - Unload plugins

    log::info("Wave shutting down");
    log::shutdown();
}

} // namespace wave
