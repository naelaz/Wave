#include <Windows.h>
#include "app/App.h"

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int
) {
    wave::App app;

    if (!app.init(hInstance)) {
        return 1;
    }

    int exitCode = app.run();
    app.shutdown();
    return exitCode;
}
