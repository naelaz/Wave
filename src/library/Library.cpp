#include "library/Library.h"
#include "metadata/Metadata.h"
#include "core/Log.h"

#include <Windows.h>
#include <algorithm>

namespace wave {

// ── Supported extensions ─────────────────────────────────────

static const wchar_t* SUPPORTED_EXTS[] = {
    L".mp3", L".flac", L".wav", L".aac", L".ogg",
    L".opus", L".m4a", L".wma", L".aiff", L".aif",
    L".alac", L".ape", L".wv", L".mka",
};

bool Library::isSupportedExtension(const std::wstring& ext) {
    std::wstring lower = ext;
    for (auto& c : lower) c = towlower(c);
    for (auto* s : SUPPORTED_EXTS) {
        if (lower == s) return true;
    }
    return false;
}

// ── Scanning ─────────────────────────────────────────────────

static std::wstring getExtension(const std::wstring& path) {
    auto dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return {};
    return path.substr(dot);
}

static std::wstring getFileName(const std::wstring& path) {
    auto slash = path.find_last_of(L"\\/");
    return (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
}

static TrackInfo buildTrackInfo(const std::wstring& fullPath, const std::wstring& fileName) {
    TrackInfo t;
    t.fullPath = fullPath;
    t.fileName = fileName;

    TrackMeta meta;
    if (readMetadata(fullPath, meta)) {
        t.title       = std::move(meta.title);
        t.artist      = std::move(meta.artist);
        t.album       = std::move(meta.album);
        t.trackNumber = meta.trackNumber;
    }
    return t;
}

static void scanRecursive(const std::wstring& dir, std::vector<TrackInfo>& out) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == L'\0' ||
             (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            continue;

        std::wstring fullPath = dir + L"\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scanRecursive(fullPath, out);
        } else {
            std::wstring ext = getExtension(fd.cFileName);
            if (Library::isSupportedExtension(ext)) {
                out.push_back(buildTrackInfo(fullPath, fd.cFileName));
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

void Library::scanFolder(const std::wstring& folderPath) {
    m_tracks.clear();
    m_playingIndex = -1;
    m_selectedIndex = 0;

    scanRecursive(folderPath, m_tracks);

    // Sort by track number if available, then by display title
    std::sort(m_tracks.begin(), m_tracks.end(),
        [](const TrackInfo& a, const TrackInfo& b) {
            if (a.trackNumber != b.trackNumber && a.trackNumber > 0 && b.trackNumber > 0)
                return a.trackNumber < b.trackNumber;
            return _wcsicmp(a.displayTitle().c_str(), b.displayTitle().c_str()) < 0;
        });

    std::string msg = "Library: scanned " + std::to_string(m_tracks.size()) + " tracks";
    log::info(msg);
}

void Library::addFile(const std::wstring& filePath) {
    std::wstring ext = getExtension(filePath);
    if (!isSupportedExtension(ext)) return;

    if (m_tracks.empty()) {
        m_playingIndex = -1;
        m_selectedIndex = 0;
    }

    m_tracks.push_back(buildTrackInfo(filePath, getFileName(filePath)));
}

void Library::clear() {
    m_tracks.clear();
    m_playingIndex = -1;
    m_selectedIndex = 0;
}

// ── Index management ─────────────────────────────────────────

void Library::setPlayingIndex(int idx) {
    if (idx >= -1 && idx < count()) m_playingIndex = idx;
}

void Library::setSelectedIndex(int idx) {
    if (count() == 0) { m_selectedIndex = 0; return; }
    if (idx < 0) idx = 0;
    if (idx >= count()) idx = count() - 1;
    m_selectedIndex = idx;
}

bool Library::hasNext() const {
    return !empty() && m_playingIndex < count() - 1;
}

bool Library::hasPrev() const {
    return !empty() && m_playingIndex > 0;
}

const TrackInfo* Library::current() const {
    return trackAt(m_playingIndex);
}

const TrackInfo* Library::next() {
    if (!hasNext()) return nullptr;
    m_playingIndex++;
    return trackAt(m_playingIndex);
}

const TrackInfo* Library::prev() {
    if (!hasPrev()) return nullptr;
    m_playingIndex--;
    return trackAt(m_playingIndex);
}

const TrackInfo* Library::trackAt(int index) const {
    if (index < 0 || index >= count()) return nullptr;
    return &m_tracks[index];
}

} // namespace wave
