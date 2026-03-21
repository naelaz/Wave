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
    m_folders.clear();
    m_playingIndex = -1;
    m_selectedIndex = 0;
    m_searchQuery.clear();

    m_folders.push_back(folderPath);
    scanRecursive(folderPath, m_tracks);
    rebuildView();

    std::string msg = "Library: scanned " + std::to_string(m_tracks.size()) + " tracks from 1 folder";
    log::info(msg);
}

void Library::addFolder(const std::wstring& folderPath) {
    // Check if already scanned
    for (auto& f : m_folders) {
        if (_wcsicmp(f.c_str(), folderPath.c_str()) == 0) {
            log::info("Library: folder already added, skipping");
            return;
        }
    }

    m_folders.push_back(folderPath);
    size_t before = m_tracks.size();
    scanRecursive(folderPath, m_tracks);
    rebuildView();

    std::string msg = "Library: added " + std::to_string(m_tracks.size() - before) +
                      " tracks from folder (" + std::to_string(m_tracks.size()) + " total)";
    log::info(msg);
}

void Library::addFile(const std::wstring& filePath) {
    std::wstring ext = getExtension(filePath);
    if (!isSupportedExtension(ext)) return;

    m_tracks.push_back(buildTrackInfo(filePath, getFileName(filePath)));
    rebuildView();
}

void Library::clear() {
    m_tracks.clear();
    m_folders.clear();
    m_viewRows.clear();
    m_playingIndex = -1;
    m_selectedIndex = 0;
    m_searchQuery.clear();
}

// ── Search ───────────────────────────────────────────────────

static std::wstring toLower(const std::wstring& s) {
    std::wstring out = s;
    for (auto& c : out) c = towlower(c);
    return out;
}

bool Library::matchesSearch(const TrackInfo& t, const std::wstring& lowerQuery) {
    if (lowerQuery.empty()) return true;
    if (toLower(t.displayTitle()).find(lowerQuery) != std::wstring::npos) return true;
    if (toLower(t.artist).find(lowerQuery) != std::wstring::npos) return true;
    if (toLower(t.album).find(lowerQuery) != std::wstring::npos) return true;
    if (toLower(t.fileName).find(lowerQuery) != std::wstring::npos) return true;
    return false;
}

void Library::setSearch(const std::wstring& query) {
    if (query == m_searchQuery) return;
    m_searchQuery = query;
    m_selectedIndex = 0;
    rebuildView();
}

void Library::setSort(SortField field, bool ascending) {
    m_sortField = field;
    m_sortAsc = ascending;
    rebuildView();
}

// ── Rebuild view with album grouping ─────────────────────────

void Library::rebuildView() {
    m_viewRows.clear();

    std::wstring lq = toLower(m_searchQuery);

    // Filter into a temp list of master indices
    std::vector<int> filtered;
    for (int i = 0; i < static_cast<int>(m_tracks.size()); i++) {
        if (matchesSearch(m_tracks[i], lq))
            filtered.push_back(i);
    }

    // Sort
    auto& tracks = m_tracks;
    SortField sf = m_sortField;
    bool asc = m_sortAsc;

    std::stable_sort(filtered.begin(), filtered.end(),
        [&](int a, int b) {
            const auto& ta = tracks[a];
            const auto& tb = tracks[b];
            int cmp = 0;
            switch (sf) {
                case SortField::Title:
                    cmp = _wcsicmp(ta.displayTitle().c_str(), tb.displayTitle().c_str());
                    break;
                case SortField::Artist:
                    cmp = _wcsicmp(ta.artist.c_str(), tb.artist.c_str());
                    if (cmp == 0) cmp = _wcsicmp(ta.displayTitle().c_str(), tb.displayTitle().c_str());
                    break;
                case SortField::Album:
                    cmp = _wcsicmp(ta.album.c_str(), tb.album.c_str());
                    if (cmp == 0) {
                        if (ta.trackNumber != tb.trackNumber)
                            cmp = ta.trackNumber - tb.trackNumber;
                        else
                            cmp = _wcsicmp(ta.displayTitle().c_str(), tb.displayTitle().c_str());
                    }
                    break;
                case SortField::TrackNumber:
                    cmp = ta.trackNumber - tb.trackNumber;
                    if (cmp == 0) cmp = _wcsicmp(ta.displayTitle().c_str(), tb.displayTitle().c_str());
                    break;
                case SortField::FileName:
                    cmp = _wcsicmp(ta.fileName.c_str(), tb.fileName.c_str());
                    break;
            }
            return asc ? (cmp < 0) : (cmp > 0);
        });

    // Build view rows with album headers when sorted by Album
    bool groupByAlbum = (sf == SortField::Album || sf == SortField::Artist);
    std::wstring lastAlbum = L"\x01"; // impossible value to force first header

    for (int idx : filtered) {
        const auto& t = tracks[idx];

        if (groupByAlbum && !m_searchQuery.empty()) {
            // Don't group when searching — flat list is more useful
            groupByAlbum = false;
        }

        if (groupByAlbum) {
            std::wstring albumKey = t.album.empty() ? L"Unknown Album" : t.album;
            if (_wcsicmp(albumKey.c_str(), lastAlbum.c_str()) != 0) {
                lastAlbum = albumKey;
                // Count tracks in this album
                int albumCount = 0;
                for (int j : filtered) {
                    const auto& other = tracks[j];
                    std::wstring otherAlbum = other.album.empty() ? L"Unknown Album" : other.album;
                    if (_wcsicmp(otherAlbum.c_str(), albumKey.c_str()) == 0) albumCount++;
                }

                ViewRow header;
                header.isHeader = true;
                header.albumName = albumKey;
                header.artistName = t.artist;
                header.trackCount = albumCount;
                m_viewRows.push_back(std::move(header));
            }
        }

        ViewRow row;
        row.isHeader = false;
        row.masterIndex = idx;
        m_viewRows.push_back(row);
    }

    if (m_selectedIndex >= count()) m_selectedIndex = std::max(0, count() - 1);
}

// ── View accessors ───────────────────────────────────────────

const ViewRow* Library::viewRowAt(int viewIndex) const {
    if (viewIndex < 0 || viewIndex >= count()) return nullptr;
    return &m_viewRows[viewIndex];
}

const TrackInfo* Library::viewTrackAt(int viewIndex) const {
    if (viewIndex < 0 || viewIndex >= count()) return nullptr;
    const auto& row = m_viewRows[viewIndex];
    if (row.isHeader) return nullptr;
    return &m_tracks[row.masterIndex];
}

int Library::viewToMaster(int viewIndex) const {
    if (viewIndex < 0 || viewIndex >= count()) return -1;
    const auto& row = m_viewRows[viewIndex];
    return row.isHeader ? -1 : row.masterIndex;
}

int Library::masterToView(int masterIndex) const {
    for (int i = 0; i < count(); i++) {
        if (!m_viewRows[i].isHeader && m_viewRows[i].masterIndex == masterIndex) return i;
    }
    return -1;
}

int Library::playingViewIndex() const {
    return masterToView(m_playingIndex);
}

// ── Index management ─────────────────────────────────────────

void Library::setPlayingIndex(int masterIdx) {
    if (masterIdx >= -1 && masterIdx < totalCount()) m_playingIndex = masterIdx;
}

void Library::setSelectedIndex(int viewIdx) {
    if (count() == 0) { m_selectedIndex = 0; return; }
    if (viewIdx < 0) viewIdx = 0;
    if (viewIdx >= count()) viewIdx = count() - 1;
    m_selectedIndex = viewIdx;
}

// Navigation: skip headers
bool Library::hasNext() const {
    int vi = playingViewIndex();
    for (int i = vi + 1; i < count(); i++) {
        if (!m_viewRows[i].isHeader) return true;
    }
    return false;
}

bool Library::hasPrev() const {
    int vi = playingViewIndex();
    for (int i = vi - 1; i >= 0; i--) {
        if (!m_viewRows[i].isHeader) return true;
    }
    return false;
}

const TrackInfo* Library::current() const {
    return trackAt(m_playingIndex);
}

const TrackInfo* Library::next() {
    int vi = playingViewIndex();
    for (int i = vi + 1; i < count(); i++) {
        if (!m_viewRows[i].isHeader) {
            m_playingIndex = m_viewRows[i].masterIndex;
            return &m_tracks[m_playingIndex];
        }
    }
    return nullptr;
}

const TrackInfo* Library::prev() {
    int vi = playingViewIndex();
    for (int i = vi - 1; i >= 0; i--) {
        if (!m_viewRows[i].isHeader) {
            m_playingIndex = m_viewRows[i].masterIndex;
            return &m_tracks[m_playingIndex];
        }
    }
    return nullptr;
}

const TrackInfo* Library::trackAt(int masterIndex) const {
    if (masterIndex < 0 || masterIndex >= totalCount()) return nullptr;
    return &m_tracks[masterIndex];
}

} // namespace wave
