#include "layout/Layout.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <Windows.h>
#include <ShlObj.h>
#include <fstream>

namespace wave {

// ── Built-in presets ─────────────────────────────────────────

LayoutPreset Layout::makeDefault() {
    LayoutPreset p;
    p.name = L"Default";
    for (int i = 0; i < PANEL_COUNT; i++) p.visible[i] = true;
    return p;
}

LayoutPreset Layout::makeMini() {
    LayoutPreset p;
    p.name = L"Mini Player";
    p.visible[static_cast<int>(PanelId::SidePanel)]     = false;
    p.visible[static_cast<int>(PanelId::NowPlaying)]    = true;
    p.visible[static_cast<int>(PanelId::TransportBar)]  = true;
    p.visible[static_cast<int>(PanelId::TransportBtns)] = true;
    p.visible[static_cast<int>(PanelId::Visualizer)]    = false;
    return p;
}

// ── Constructor ──────────────────────────────────────────────

Layout::Layout() {
    resetToDefault();
}

void Layout::resetToDefault() {
    m_presets.clear();
    m_presets.push_back(makeDefault());
    m_presets.push_back(makeMini());
    applyPreset(0);
}

// ── Visibility ───────────────────────────────────────────────

bool Layout::isPanelVisible(PanelId id) const {
    int i = static_cast<int>(id);
    return (i >= 0 && i < PANEL_COUNT) ? m_visible[i] : false;
}

void Layout::setPanelVisible(PanelId id, bool vis) {
    int i = static_cast<int>(id);
    if (i >= 0 && i < PANEL_COUNT) m_visible[i] = vis;
}

void Layout::togglePanel(PanelId id) {
    setPanelVisible(id, !isPanelVisible(id));
}

float Layout::sidePanelWidth() const {
    return isPanelVisible(PanelId::SidePanel) ? DEFAULT_PANEL_WIDTH : 0.0f;
}

// ── Presets ──────────────────────────────────────────────────

void Layout::applyPreset(int index) {
    if (index < 0 || index >= static_cast<int>(m_presets.size())) return;
    m_activePreset = index;
    for (int i = 0; i < PANEL_COUNT; i++)
        m_visible[i] = m_presets[index].visible[i];
}

int Layout::saveCurrentAsPreset(const std::wstring& name) {
    LayoutPreset p;
    p.name = name;
    for (int i = 0; i < PANEL_COUNT; i++)
        p.visible[i] = m_visible[i];
    m_presets.push_back(p);
    save();
    return static_cast<int>(m_presets.size()) - 1;
}

void Layout::deletePreset(int index) {
    // Don't delete built-in presets (first two)
    if (index < 2 || index >= static_cast<int>(m_presets.size())) return;
    m_presets.erase(m_presets.begin() + index);
    if (m_activePreset >= static_cast<int>(m_presets.size()))
        m_activePreset = 0;
    save();
}

// ── Persistence ──────────────────────────────────────────────

std::wstring Layout::filePath() {
    // Delegate to Settings::dataDir() for portable/normal mode
    // We can't include Settings.h here without circular deps, so replicate the logic
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring eDir(exePath);
    auto s = eDir.find_last_of(L"\\/");
    if (s != std::wstring::npos) eDir = eDir.substr(0, s);

    std::wstring marker = eDir + L"\\portable.txt";
    if (GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring dir = eDir + L"\\data";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\layout.json";
    }

    wchar_t* appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        return L"layout.json";
    std::wstring dir = appData;
    CoTaskMemFree(appData);
    dir += L"\\Wave";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\layout.json";
}

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else o += c;
    }
    return o;
}

void Layout::save() const {
    std::ofstream f(filePath(), std::ios::binary);
    if (!f) return;

    f << "{\n";

    // Current visibility
    f << "  \"visible\": [";
    for (int i = 0; i < PANEL_COUNT; i++) {
        f << (m_visible[i] ? "true" : "false");
        if (i + 1 < PANEL_COUNT) f << ", ";
    }
    f << "],\n";
    f << "  \"activePreset\": " << m_activePreset << ",\n";

    // User presets (skip first two built-ins)
    f << "  \"userPresets\": [\n";
    bool first = true;
    for (size_t i = 2; i < m_presets.size(); i++) {
        if (!first) f << ",\n";
        first = false;
        f << "    {\"name\": \"" << esc(platform::toUtf8(m_presets[i].name)) << "\", \"v\": [";
        for (int j = 0; j < PANEL_COUNT; j++) {
            f << (m_presets[i].visible[j] ? "1" : "0");
            if (j + 1 < PANEL_COUNT) f << ",";
        }
        f << "]}";
    }
    f << "\n  ]\n}\n";
}

// Minimal reader
static void skip(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) p++;
}

static std::string rstr(const std::string& s, size_t& p) {
    skip(s, p);
    if (p >= s.size() || s[p] != '"') return {};
    p++;
    std::string o;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) { p++; o += s[p]; }
        else o += s[p];
        p++;
    }
    if (p < s.size()) p++;
    return o;
}

void Layout::load() {
    std::ifstream f(filePath(), std::ios::binary);
    if (!f) return;

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    f.close();

    // Reset to defaults first, then overlay
    resetToDefault();

    size_t p = 0;
    skip(json, p);
    if (p >= json.size() || json[p] != '{') return;
    p++;

    while (p < json.size() && json[p] != '}') {
        skip(json, p);
        if (json[p] == ',' || json[p] == '}') { if (json[p] == ',') p++; continue; }

        std::string key = rstr(json, p);
        skip(json, p);
        if (p < json.size() && json[p] == ':') p++;
        skip(json, p);

        if (key == "visible" && json[p] == '[') {
            p++;
            for (int i = 0; i < PANEL_COUNT && p < json.size(); i++) {
                skip(json, p);
                if (json[p] == ',') { p++; skip(json, p); }
                if (json[p] == 't') { m_visible[i] = true; p += 4; }
                else if (json[p] == 'f') { m_visible[i] = false; p += 5; }
            }
            skip(json, p);
            if (p < json.size() && json[p] == ']') p++;
        }
        else if (key == "activePreset") {
            int val = 0;
            while (p < json.size() && json[p] >= '0' && json[p] <= '9') {
                val = val * 10 + (json[p] - '0'); p++;
            }
            m_activePreset = val;
        }
        else if (key == "userPresets" && json[p] == '[') {
            p++;
            while (p < json.size() && json[p] != ']') {
                skip(json, p);
                if (json[p] == ',' || json[p] == ']') { if (json[p] == ',') p++; continue; }
                if (json[p] != '{') break;
                p++;

                LayoutPreset lp;
                while (p < json.size() && json[p] != '}') {
                    skip(json, p);
                    if (json[p] == ',' || json[p] == '}') { if (json[p] == ',') p++; continue; }
                    std::string k2 = rstr(json, p);
                    skip(json, p);
                    if (p < json.size() && json[p] == ':') p++;
                    skip(json, p);

                    if (k2 == "name") {
                        lp.name = platform::toWide(rstr(json, p));
                    } else if (k2 == "v" && json[p] == '[') {
                        p++;
                        for (int i = 0; i < PANEL_COUNT && p < json.size(); i++) {
                            skip(json, p);
                            if (json[p] == ',') { p++; skip(json, p); }
                            lp.visible[i] = (json[p] == '1');
                            p++;
                        }
                        skip(json, p);
                        if (p < json.size() && json[p] == ']') p++;
                    }
                    skip(json, p);
                }
                if (p < json.size()) p++; // skip }

                if (!lp.name.empty()) m_presets.push_back(std::move(lp));
                skip(json, p);
            }
            if (p < json.size()) p++; // skip ]
        }
        else {
            // skip unknown value
            while (p < json.size() && json[p] != ',' && json[p] != '}') p++;
        }
        skip(json, p);
    }

    // Clamp active preset
    if (m_activePreset < 0 || m_activePreset >= static_cast<int>(m_presets.size()))
        m_activePreset = 0;

    log::info("Layout loaded");
}

} // namespace wave
