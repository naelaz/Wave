#include "core/Log.h"

#include <Windows.h>
#include <ShlObj.h>
#include <string>
#include <fstream>
#include <mutex>

namespace wave::log {

static std::ofstream s_logFile;
static std::mutex s_mutex;
static bool s_fileReady = false;

static std::wstring logFilePath() {
    // Check portable mode: if "portable.txt" exists next to exe, log there
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    auto slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash);

    std::wstring portableMarker = dir + L"\\portable.txt";
    if (GetFileAttributesW(portableMarker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW((dir + L"\\data").c_str(), nullptr);
        return dir + L"\\data\\wave.log";
    }

    // Normal mode: %APPDATA%\Wave
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::wstring path = appData;
        CoTaskMemFree(appData);
        path += L"\\Wave";
        CreateDirectoryW(path.c_str(), nullptr);
        return path + L"\\wave.log";
    }
    return dir + L"\\wave.log";
}

void init() {
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_fileReady) {
            s_logFile.open(logFilePath(), std::ios::trunc);
            s_fileReady = s_logFile.is_open();
        }
    }
    info("Wave logger initialized");
}

void shutdown() {
    info("Wave logger shutting down");
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_logFile.is_open()) s_logFile.close();
        s_fileReady = false;
    }
}

static void output(std::string_view prefix, std::string_view msg) {
    // Timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d.%03d",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::string line;
    line.reserve(prefix.size() + msg.size() + 32);
    line += timeBuf;
    line += " [";
    line += prefix;
    line += "] ";
    line += msg;
    line += "\n";

    // Debug output (visible in VS/DebugView)
    OutputDebugStringA(line.c_str());

    // File output
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_fileReady) {
            s_logFile << line;
            s_logFile.flush();
        }
    }
}

void info(std::string_view msg)  { output("INFO",  msg); }
void warn(std::string_view msg)  { output("WARN",  msg); }
void error(std::string_view msg) { output("ERROR", msg); }

} // namespace wave::log
