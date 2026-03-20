#pragma once

#include "library/Library.h"
#include <string>
#include <vector>

namespace wave {

struct Playlist {
    std::wstring name;
    std::vector<TrackInfo> tracks;
    int playingIndex = -1;
};

class PlaylistManager {
public:
    // ── Playlists ────────────────────────────────────────────
    int createPlaylist(const std::wstring& name);
    void deletePlaylist(int index);
    void addToPlaylist(int playlistIdx, const TrackInfo& track);
    void removeFromPlaylist(int playlistIdx, int trackIdx);

    const std::vector<Playlist>& playlists() const { return m_playlists; }
    Playlist* playlist(int index);
    int playlistCount() const { return static_cast<int>(m_playlists.size()); }

    // ── Queue ────────────────────────────────────────────────
    void enqueue(const TrackInfo& track);
    void removeFromQueue(int index);
    void clearQueue();
    const std::vector<TrackInfo>& queue() const { return m_queue; }
    int queueCount() const { return static_cast<int>(m_queue.size()); }

    // Pop and return the first queued track (caller takes ownership of data)
    bool dequeueNext(TrackInfo& out);

    // ── Persistence ──────────────────────────────────────────
    void save();
    void load();

private:
    static std::wstring appDataDir();

    std::vector<Playlist> m_playlists;
    std::vector<TrackInfo> m_queue;
};

} // namespace wave
