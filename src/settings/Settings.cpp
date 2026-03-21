#include "settings/Settings.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <string>
#include <cstdlib>

namespace wave {

// ── Path ─────────────────────────────────────────────────────

static std::wstring exeDir() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring dir(buf);
    auto s = dir.find_last_of(L"\\/");
    return (s != std::wstring::npos) ? dir.substr(0, s) : dir;
}

bool Settings::isPortable() {
    std::wstring marker = exeDir() + L"\\portable.txt";
    return GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Settings::dataDir() {
    if (isPortable()) {
        std::wstring dir = exeDir() + L"\\data";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    wchar_t* appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        return exeDir();
    std::wstring dir = appData;
    CoTaskMemFree(appData);
    dir += L"\\Wave";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring Settings::filePath() {
    return dataDir() + L"\\settings.json";
}

// ── Minimal JSON helpers ─────────────────────────────────────

static std::string escJson(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else o += c;
    }
    return o;
}

static void skipWs(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) p++;
}

static std::string readStr(const std::string& s, size_t& p) {
    skipWs(s, p);
    if (p >= s.size() || s[p] != '"') return {};
    p++;
    std::string out;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) { p++; out += s[p]; }
        else out += s[p];
        p++;
    }
    if (p < s.size()) p++;
    return out;
}

static std::string readValue(const std::string& s, size_t& p) {
    skipWs(s, p);
    if (p >= s.size()) return {};
    if (s[p] == '"') return readStr(s, p);
    // Read raw value (number, bool)
    size_t start = p;
    while (p < s.size() && s[p] != ',' && s[p] != '}' && s[p] != '\n' && s[p] != '\r')
        p++;
    std::string val = s.substr(start, p - start);
    // Trim trailing whitespace
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

// ── Save ─────────────────────────────────────────────────────

void Settings::save() const {
    std::ofstream f(filePath(), std::ios::binary);
    if (!f) return;

    f << "{\n";
    f << "  \"windowX\": "      << windowX << ",\n";
    f << "  \"windowY\": "      << windowY << ",\n";
    f << "  \"windowWidth\": "  << windowWidth << ",\n";
    f << "  \"windowHeight\": " << windowHeight << ",\n";
    f << "  \"maximized\": "    << (maximized ? "true" : "false") << ",\n";
    f << "  \"volume\": "       << static_cast<int>(volume) << ",\n";
    f << "  \"lastFolder\": \"" << escJson(platform::toUtf8(lastFolder)) << "\",\n";
    f << "  \"themePreset\": \"" << escJson(platform::toUtf8(themePreset)) << "\",\n";
    f << "  \"themeAccent\": " << themeAccent << ",\n";
    f << "  \"visualizerMode\": " << visualizerMode << ",\n";
    f << "  \"audioBackend\": " << audioBackend << ",\n";
    f << "  \"audioDevice\": \"" << escJson(audioDevice) << "\",\n";
    f << "  \"gapless\": " << (gapless ? "true" : "false") << ",\n";
    f << "  \"replayGain\": " << replayGain << ",\n";
    f << "  \"replayGainPreamp\": " << static_cast<int>(replayGainPreamp) << ",\n";
    f << "  \"startPaused\": " << (startPaused ? "true" : "false") << ",\n";
    f << "  \"crossfadeMs\": " << crossfadeMs << ",\n";
    f << "  \"repeatMode\": " << repeatMode << ",\n";
    f << "  \"shuffle\": " << (shuffle ? "true" : "false") << ",\n";
    f << "  \"eqData\": \"" << escJson(eqData) << "\",\n";
    f << "  \"coverFlowMode\": " << (coverFlowMode ? "true" : "false") << "\n";
    f << "}\n";

    log::info("Settings saved");
}

// ── Load ─────────────────────────────────────────────────────

void Settings::load() {
    std::ifstream f(filePath(), std::ios::binary);
    if (!f) {
        log::info("No settings file found, using defaults");
        return;
    }

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    f.close();

    size_t p = 0;
    skipWs(json, p);
    if (p >= json.size() || json[p] != '{') {
        log::warn("Settings file corrupt, using defaults");
        return;
    }
    p++;

    while (p < json.size() && json[p] != '}') {
        skipWs(json, p);
        if (json[p] == ',' || json[p] == '}') { if (json[p] == ',') p++; continue; }

        std::string key = readStr(json, p);
        skipWs(json, p);
        if (p < json.size() && json[p] == ':') p++;

        std::string val = readValue(json, p);

        if      (key == "windowX")      windowX      = std::atoi(val.c_str());
        else if (key == "windowY")      windowY      = std::atoi(val.c_str());
        else if (key == "windowWidth")  windowWidth  = std::atoi(val.c_str());
        else if (key == "windowHeight") windowHeight = std::atoi(val.c_str());
        else if (key == "maximized")    maximized    = (val == "true");
        else if (key == "volume")       volume       = std::atof(val.c_str());
        else if (key == "lastFolder")   lastFolder   = platform::toWide(val);
        else if (key == "themePreset") themePreset  = platform::toWide(val);
        else if (key == "themeAccent")    themeAccent    = std::atoi(val.c_str());
        else if (key == "visualizerMode") visualizerMode = std::atoi(val.c_str());
        else if (key == "audioBackend")     audioBackend     = std::atoi(val.c_str());
        else if (key == "audioDevice")      audioDevice      = val;
        else if (key == "gapless")          gapless          = (val == "true");
        else if (key == "replayGain")       replayGain       = std::atoi(val.c_str());
        else if (key == "replayGainPreamp") replayGainPreamp = std::atof(val.c_str());
        else if (key == "startPaused")      startPaused      = (val == "true");
        else if (key == "crossfadeMs")      crossfadeMs      = std::atoi(val.c_str());
        else if (key == "repeatMode")       repeatMode       = std::atoi(val.c_str());
        else if (key == "shuffle")          shuffle          = (val == "true");
        else if (key == "eqData")           eqData           = val;
        else if (key == "coverFlowMode")    coverFlowMode    = (val == "true");

        skipWs(json, p);
    }

    // Validate window dimensions
    if (windowWidth < 400) windowWidth = 400;
    if (windowHeight < 300) windowHeight = 300;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    log::info("Settings loaded");
}

} // namespace wave
