#pragma once

#include <string>
#include <vector>

namespace wave {

struct TrackInfo {
    std::wstring fullPath;
    std::wstring fileName;

    // Metadata (empty if unavailable)
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    int trackNumber = 0;

    // Display helpers — return metadata with filename fallback
    const std::wstring& displayTitle() const {
        return title.empty() ? fileName : title;
    }
    std::wstring displayArtistAlbum() const {
        if (!artist.empty() && !album.empty()) return artist + L" \u2014 " + album;
        if (!artist.empty()) return artist;
        if (!album.empty()) return album;
        return {};
    }
};

enum class SortField {
    Title,
    Artist,
    Album,
    TrackNumber,
    FileName,
};

// A row in the view: either an album header or a track
struct ViewRow {
    bool isHeader = false;        // true = album header, false = track
    int masterIndex = -1;         // index into m_tracks (only valid if !isHeader)
    std::wstring albumName;       // only valid if isHeader
    std::wstring artistName;      // only valid if isHeader
    int trackCount = 0;           // only valid if isHeader
};

class Library {
public:
    void scanFolder(const std::wstring& folderPath);  // replaces library
    void addFolder(const std::wstring& folderPath);   // appends to library
    void addFile(const std::wstring& filePath);
    void clear();

    // Scanned folders
    const std::vector<std::wstring>& folders() const { return m_folders; }

    // Full (unfiltered) track list
    const std::vector<TrackInfo>& tracks() const { return m_tracks; }
    int totalCount() const { return static_cast<int>(m_tracks.size()); }

    // Filtered/sorted view — UI should use these
    int count() const { return static_cast<int>(m_viewRows.size()); }
    bool empty() const { return m_viewRows.empty(); }
    const ViewRow* viewRowAt(int viewIndex) const;
    const TrackInfo* viewTrackAt(int viewIndex) const; // returns nullptr for headers
    int viewToMaster(int viewIndex) const; // -1 for headers
    int masterToView(int masterIndex) const;

    // Search
    void setSearch(const std::wstring& query);
    const std::wstring& searchQuery() const { return m_searchQuery; }

    // Sort
    void setSort(SortField field, bool ascending = true);
    SortField sortField() const { return m_sortField; }
    bool sortAscending() const { return m_sortAsc; }

    // Playing index (in master list)
    int playingIndex() const { return m_playingIndex; }
    void setPlayingIndex(int masterIdx);

    // Selected index (in view list)
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int viewIdx);

    // Navigation (operates on filtered view)
    bool hasNext() const;
    bool hasPrev() const;
    const TrackInfo* current() const;
    const TrackInfo* next();
    const TrackInfo* prev();
    const TrackInfo* trackAt(int masterIndex) const;

    // View-based navigation for playback started from filtered results
    int playingViewIndex() const;

    static bool isSupportedExtension(const std::wstring& ext);

private:
    void rebuildView();
    static bool matchesSearch(const TrackInfo& t, const std::wstring& lowerQuery);

    std::vector<TrackInfo> m_tracks;
    std::vector<std::wstring> m_folders;
    std::vector<ViewRow> m_viewRows;

    int m_playingIndex = -1;  // master index
    int m_selectedIndex = 0;  // view index

    std::wstring m_searchQuery;
    SortField m_sortField = SortField::Album; // default: group by album
    bool m_sortAsc = true;
};

} // namespace wave
