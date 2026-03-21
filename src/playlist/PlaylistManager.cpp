#include "playlist/PlaylistManager.h"
#include "metadata/Metadata.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <sstream>

namespace wave {

// ── Playlists ────────────────────────────────────────────────

int PlaylistManager::createPlaylist(const std::wstring& name) {
    m_playlists.push_back({ name, {}, -1 });
    log::info("Created playlist: " + platform::toUtf8(name));
    save();
    return static_cast<int>(m_playlists.size()) - 1;
}

void PlaylistManager::deletePlaylist(int index) {
    if (index < 0 || index >= playlistCount()) return;
    m_playlists.erase(m_playlists.begin() + index);
    save();
}

void PlaylistManager::addToPlaylist(int playlistIdx, const TrackInfo& track) {
    if (playlistIdx < 0 || playlistIdx >= playlistCount()) return;
    m_playlists[playlistIdx].tracks.push_back(track);
    save();
}

void PlaylistManager::removeFromPlaylist(int playlistIdx, int trackIdx) {
    if (playlistIdx < 0 || playlistIdx >= playlistCount()) return;
    auto& tracks = m_playlists[playlistIdx].tracks;
    if (trackIdx < 0 || trackIdx >= static_cast<int>(tracks.size())) return;
    tracks.erase(tracks.begin() + trackIdx);
    save();
}

Playlist* PlaylistManager::playlist(int index) {
    if (index < 0 || index >= playlistCount()) return nullptr;
    return &m_playlists[index];
}

// ── Queue ────────────────────────────────────────────────────

void PlaylistManager::enqueue(const TrackInfo& track) {
    m_queue.push_back(track);
}

void PlaylistManager::removeFromQueue(int index) {
    if (index < 0 || index >= queueCount()) return;
    m_queue.erase(m_queue.begin() + index);
}

void PlaylistManager::clearQueue() {
    m_queue.clear();
}

bool PlaylistManager::dequeueNext(TrackInfo& out) {
    if (m_queue.empty()) return false;
    out = std::move(m_queue.front());
    m_queue.erase(m_queue.begin());
    return true;
}

// ── Persistence ──────────────────────────────────────────────

std::wstring PlaylistManager::appDataDir() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring eDir(exePath);
    auto s = eDir.find_last_of(L"\\/");
    if (s != std::wstring::npos) eDir = eDir.substr(0, s);

    std::wstring marker = eDir + L"\\portable.txt";
    if (GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring dir = eDir + L"\\data";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }

    wchar_t* appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        return L".";
    std::wstring dir = appData;
    CoTaskMemFree(appData);
    dir += L"\\Wave";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

// Minimal JSON writer
static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

void PlaylistManager::save() {
    std::wstring path = appDataDir() + L"\\playlists.json";
    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    f << "[\n";
    for (size_t i = 0; i < m_playlists.size(); i++) {
        auto& pl = m_playlists[i];
        f << "  {\"name\": \"" << escapeJson(platform::toUtf8(pl.name)) << "\",\n";
        f << "   \"tracks\": [\n";
        for (size_t j = 0; j < pl.tracks.size(); j++) {
            f << "    \"" << escapeJson(platform::toUtf8(pl.tracks[j].fullPath)) << "\"";
            if (j + 1 < pl.tracks.size()) f << ",";
            f << "\n";
        }
        f << "   ]}";
        if (i + 1 < m_playlists.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
}

// Minimal JSON reader — handles our specific schema only
static void skipWs(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) p++;
}

static std::string readJsonString(const std::string& s, size_t& p) {
    skipWs(s, p);
    if (p >= s.size() || s[p] != '"') return {};
    p++; // skip opening quote
    std::string out;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) {
            p++;
            switch (s[p]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[p]; break;
            }
        } else {
            out += s[p];
        }
        p++;
    }
    if (p < s.size()) p++; // skip closing quote
    return out;
}

static TrackInfo buildTrackFromPath(const std::wstring& path) {
    TrackInfo t;
    t.fullPath = path;
    // Extract filename
    auto slash = path.find_last_of(L"\\/");
    t.fileName = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
    // Read metadata
    TrackMeta meta;
    if (readMetadata(path, meta)) {
        t.title       = std::move(meta.title);
        t.artist      = std::move(meta.artist);
        t.album       = std::move(meta.album);
        t.trackNumber = meta.trackNumber;
    }
    return t;
}

void PlaylistManager::load() {
    std::wstring path = appDataDir() + L"\\playlists.json";
    std::ifstream f(path, std::ios::binary);
    if (!f) return;

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    f.close();

    m_playlists.clear();
    size_t p = 0;
    skipWs(json, p);
    if (p >= json.size() || json[p] != '[') return;
    p++; // skip [

    while (p < json.size()) {
        skipWs(json, p);
        if (p >= json.size() || json[p] == ']') break;
        if (json[p] == ',') { p++; continue; }
        if (json[p] != '{') break;
        p++; // skip {

        Playlist pl;
        while (p < json.size() && json[p] != '}') {
            skipWs(json, p);
            if (json[p] == ',' || json[p] == '}') { if (json[p] == ',') p++; continue; }

            std::string key = readJsonString(json, p);
            skipWs(json, p);
            if (p < json.size() && json[p] == ':') p++;

            if (key == "name") {
                pl.name = platform::toWide(readJsonString(json, p));
            } else if (key == "tracks") {
                skipWs(json, p);
                if (p < json.size() && json[p] == '[') {
                    p++; // skip [
                    while (p < json.size() && json[p] != ']') {
                        skipWs(json, p);
                        if (json[p] == ',') { p++; continue; }
                        if (json[p] == ']') break;
                        std::string trackPath = readJsonString(json, p);
                        if (!trackPath.empty()) {
                            std::wstring wpath = platform::toWide(trackPath);
                            // Only add if file still exists
                            if (GetFileAttributesW(wpath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                pl.tracks.push_back(buildTrackFromPath(wpath));
                            }
                        }
                        skipWs(json, p);
                    }
                    if (p < json.size()) p++; // skip ]
                }
            }
            skipWs(json, p);
        }
        if (p < json.size()) p++; // skip }

        if (!pl.name.empty()) {
            m_playlists.push_back(std::move(pl));
        }
        skipWs(json, p);
    }

    std::string msg = "Loaded " + std::to_string(m_playlists.size()) + " playlists";
    log::info(msg);
}

} // namespace wave
