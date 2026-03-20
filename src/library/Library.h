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

class Library {
public:
    void scanFolder(const std::wstring& folderPath);
    void addFile(const std::wstring& filePath);
    void clear();

    const std::vector<TrackInfo>& tracks() const { return m_tracks; }
    int count() const { return static_cast<int>(m_tracks.size()); }
    bool empty() const { return m_tracks.empty(); }

    int playingIndex() const { return m_playingIndex; }
    void setPlayingIndex(int idx);

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int idx);

    bool hasNext() const;
    bool hasPrev() const;
    const TrackInfo* current() const;
    const TrackInfo* next();
    const TrackInfo* prev();
    const TrackInfo* trackAt(int index) const;

    static bool isSupportedExtension(const std::wstring& ext);

private:
    std::vector<TrackInfo> m_tracks;
    int m_playingIndex = -1;
    int m_selectedIndex = 0;
};

} // namespace wave
