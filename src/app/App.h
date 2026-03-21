#pragma once

#include <Windows.h>
#include "window/MainWindow.h"
#include "audio/Engine.h"
#include "ui/Renderer.h"
#include "library/Library.h"
#include "playlist/PlaylistManager.h"
#include "settings/Settings.h"
#include "layout/Layout.h"
#include "theme/Theme.h"
#include "visualizer/Visualizer.h"
#include "audio/AudioCapture.h"
#include "plugin/PluginHost.h"
#include "audio/WaveformCache.h"
#include "art/AlbumArt.h"
#include "coverflow/CoverFlow.h"
#include "audio/Equalizer.h"

namespace wave {

enum class PlaybackSource { Library, Playlist, Queue };
enum class RepeatMode { Off, One, All };

class App {
public:
    bool init(HINSTANCE hInstance);
    int run();
    void shutdown();

    // File/folder open
    void openFile();
    void openFolder();
    void addFolderToLibrary();

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
    Settings& settings() { return m_settings; }
    Layout& layout() { return m_layout; }
    Theme& theme() { return m_theme; }
    Visualizer& visualizer() { return m_visualizer; }
    PluginHost& pluginHost() { return m_pluginHost; }
    CoverFlow& coverFlow() { return m_coverFlow; }
    Equalizer& equalizer() { return m_equalizer; }
    PlaybackSource playbackSource() const { return m_playSource; }
    int activePlaylistIndex() const { return m_activePlaylistIdx; }
    RepeatMode repeatMode() const { return m_repeatMode; }
    void cycleRepeat();
    bool shuffle() const { return m_shuffle; }
    void toggleShuffle();

    // Cover Flow: play first track of focused album
    void playCoverFlowAlbum();

private:
    void playTrackInternal(const TrackInfo* track);

    HINSTANCE m_hInstance = nullptr;
    MainWindow m_mainWindow;
    Engine m_engine;
    Renderer m_renderer;
    Library m_library;
    PlaylistManager m_playlistMgr;
    Settings m_settings;
    Layout m_layout;
    Theme m_theme;
    Visualizer m_visualizer;
    AudioCapture m_audioCapture;
    PluginHost m_pluginHost;
    WaveformCache m_waveformCache;
    AlbumArt m_albumArt;
    CoverFlow m_coverFlow;
    Equalizer m_equalizer;

    PlaybackSource m_playSource = PlaybackSource::Library;
    int m_activePlaylistIdx = -1;
    RepeatMode m_repeatMode = RepeatMode::Off;
    bool m_shuffle = false;
};

} // namespace wave
