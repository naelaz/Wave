#pragma once

#include <Windows.h>
#include "window/MainWindow.h"
#include "audio/Engine.h"
#include "ui/Renderer.h"
#include "library/Library.h"
#include "playlist/PlaylistManager.h"

namespace wave {

enum class PlaybackSource { Library, Playlist, Queue };

class App {
public:
    bool init(HINSTANCE hInstance);
    int run();
    void shutdown();

    // File/folder open
    void openFile();
    void openFolder();

    // Playback through library
    void playTrack(int index);
    void playNext();
    void playPrev();

    // Playback through playlist
    void playPlaylistTrack(int playlistIdx, int trackIdx);

    // Playlist management (called by MainWindow menu/context)
    void createPlaylist();
    void deletePlaylist(int index);
    void addTrackToPlaylist(int playlistIdx, const TrackInfo& track);
    void addTrackToQueue(const TrackInfo& track);

    // Track end handler
    void onTrackEnded();

    // Accessors
    Library& library() { return m_library; }
    Engine&  engine()  { return m_engine; }
    Renderer& renderer() { return m_renderer; }
    PlaylistManager& playlistMgr() { return m_playlistMgr; }
    PlaybackSource playbackSource() const { return m_playSource; }
    int activePlaylistIndex() const { return m_activePlaylistIdx; }

private:
    void playTrackInternal(const TrackInfo* track);

    HINSTANCE m_hInstance = nullptr;
    MainWindow m_mainWindow;
    Engine m_engine;
    Renderer m_renderer;
    Library m_library;
    PlaylistManager m_playlistMgr;

    PlaybackSource m_playSource = PlaybackSource::Library;
    int m_activePlaylistIdx = -1;
};

} // namespace wave
