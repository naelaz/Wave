#include "app/App.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <algorithm>
#include <random>
#include <shellapi.h>
#include <shobjidl.h>
#include <commdlg.h>

namespace wave {

bool App::init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    log::init();
    log::info("Wave v0.1.0 starting");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Load settings, layout, and apply theme from saved prefs
    m_settings.load();
    m_layout.load();
    m_theme.applyPresetById(m_settings.themePreset);
    m_theme.setAccent(m_settings.themeAccent);

    if (!m_mainWindow.create(hInstance, this, m_settings)) {
        log::error("Failed to initialize main window");
        return false;
    }

    // Restore visualizer mode and playback modes from settings
    m_visualizer.setMode(static_cast<VisMode>(m_settings.visualizerMode));
    m_repeatMode = static_cast<RepeatMode>(std::clamp(m_settings.repeatMode, 0, 2));
    m_shuffle = m_settings.shuffle;
    m_equalizer.deserialize(m_settings.eqData);
    m_visualizer.setCapture(&m_audioCapture);
    m_audioCapture.start();

    if (!m_renderer.init(m_mainWindow.handle(), &m_engine, &m_library, &m_playlistMgr, &m_layout, &m_theme, &m_visualizer, &m_albumArt, &m_waveformCache)) {
        log::error("Failed to initialize renderer");
        return false;
    }
    m_renderer.setCoverFlow(&m_coverFlow);
    m_renderer.setCoverFlowMode(m_settings.coverFlowMode);
    m_mainWindow.setRenderer(&m_renderer);

    // Build audio settings from saved preferences
    AudioSettings audioConfig;
    audioConfig.backend = static_cast<AudioBackend>(std::clamp(m_settings.audioBackend, 0, 2));
    audioConfig.deviceId = m_settings.audioDevice;
    audioConfig.gapless = m_settings.gapless;
    audioConfig.replayGain = static_cast<ReplayGainMode>(std::clamp(m_settings.replayGain, 0, 2));
    audioConfig.replayGainPreamp = m_settings.replayGainPreamp;

    if (!m_engine.init(m_mainWindow.handle(), audioConfig)) {
        log::warn("Audio engine not available (is libmpv-2.dll next to Wave.exe?)");
    }

    // Restore volume and EQ from settings
    m_engine.setVolume(m_settings.volume);
    m_equalizer.applyToEngine(&m_engine);

    m_engine.setTrackEndCallback([](void* ctx) {
        static_cast<App*>(ctx)->onTrackEnded();
    }, this);

    // Load saved playlists
    m_playlistMgr.load();

    // Command-line argument takes priority over saved folder
    bool loadedFromCmdLine = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1) {
            std::wstring wpath = argv[1];
            DWORD attr = GetFileAttributesW(wpath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                m_library.scanFolder(wpath);
                m_settings.lastFolder = wpath;
                if (!m_library.empty()) playTrack(0);
            } else {
                m_library.addFile(wpath);
                playTrack(0);
            }
            loadedFromCmdLine = true;
        }
        LocalFree(argv);
    }

    // Reopen last folder if no command-line argument
    if (!loadedFromCmdLine && !m_settings.lastFolder.empty()) {
        DWORD attr = GetFileAttributesW(m_settings.lastFolder.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            m_library.scanFolder(m_settings.lastFolder);
            log::info("Reopened last folder");
        } else {
            m_settings.lastFolder.clear();
            log::info("Last folder no longer exists, skipped");
        }
    }

    // Rebuild cover flow from loaded library
    m_coverFlow.rebuild(&m_library);

    // Initialize plugin system
    m_pluginHost.setApp(this);
    m_pluginHost.init();
    m_mainWindow.refreshPluginMenu();
    m_mainWindow.refreshAudioMenu();

    log::info("Initialization complete");
    return true;
}

int App::run() {
    m_mainWindow.show(m_settings);
    return platform::runMessageLoop();
}

void App::shutdown() {
    // Save window state before shutdown
    m_mainWindow.saveWindowState(m_settings);
    m_settings.volume = m_engine.volume();
    m_settings.themePreset = m_theme.presetId();
    m_settings.themeAccent = m_theme.accentIndex();
    m_settings.visualizerMode = static_cast<int>(m_visualizer.mode());
    m_settings.repeatMode = static_cast<int>(m_repeatMode);
    m_settings.shuffle = m_shuffle;
    m_settings.eqData = m_equalizer.serialize();
    m_settings.coverFlowMode = m_renderer.isCoverFlowMode();
    m_settings.save();
    m_layout.save();

    m_waveformCache.cancel(); // stop background scan before engine shutdown
    m_pluginHost.shutdown();
    m_audioCapture.stop();
    m_playlistMgr.save();
    m_engine.shutdown();
    m_renderer.shutdown();
    CoUninitialize();
    log::info("Wave shutting down");
    log::shutdown();
}

// ── File/folder dialogs ──────────────────────────────────────

void App::openFile() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_mainWindow.handle();
    ofn.lpstrFilter  = L"Audio Files\0*.mp3;*.flac;*.wav;*.aac;*.ogg;*.opus;*.m4a;*.wma;*.aiff;*.aif;*.ape;*.wv;*.mka\0All Files\0*.*\0";
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        m_library.clear();
        m_library.addFile(file);
        m_playSource = PlaybackSource::Library;
        m_settings.lastFolder.clear(); // single file, no folder to remember
        playTrack(0);
    }
}

void App::openFolder() {
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileDialog, reinterpret_cast<void**>(&pfd));
    if (FAILED(hr)) return;

    DWORD options = 0;
    pfd->GetOptions(&options);
    pfd->SetOptions(options | FOS_PICKFOLDERS);
    pfd->SetTitle(L"Open Music Folder");

    hr = pfd->Show(m_mainWindow.handle());
    if (SUCCEEDED(hr)) {
        IShellItem* psi = nullptr;
        pfd->GetResult(&psi);
        if (psi) {
            PWSTR path = nullptr;
            psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (path) {
                m_library.scanFolder(path);
                m_settings.lastFolder = path; // remember for next launch
                m_playSource = PlaybackSource::Library;
                m_coverFlow.rebuild(&m_library);
                if (!m_library.empty()) playTrack(0);
                CoTaskMemFree(path);
            }
            psi->Release();
        }
    }
    pfd->Release();
}

void App::addFolderToLibrary() {
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pfd));
    if (FAILED(hr)) return;

    DWORD options = 0;
    pfd->GetOptions(&options);
    pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT);
    pfd->SetTitle(L"Add Music Folders (select multiple with Ctrl+Click)");

    hr = pfd->Show(m_mainWindow.handle());
    if (SUCCEEDED(hr)) {
        IShellItemArray* results = nullptr;
        pfd->GetResults(&results);
        if (results) {
            DWORD count = 0;
            results->GetCount(&count);
            for (DWORD i = 0; i < count; i++) {
                IShellItem* psi = nullptr;
                results->GetItemAt(i, &psi);
                if (psi) {
                    PWSTR path = nullptr;
                    psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
                    if (path) {
                        m_library.addFolder(path);
                        log::info("Added folder: " + platform::toUtf8(path));
                        CoTaskMemFree(path);
                    }
                    psi->Release();
                }
            }
            results->Release();
            m_playSource = PlaybackSource::Library;
            m_coverFlow.rebuild(&m_library);
        }
    }
    pfd->Release();
}

// ── Playback ─────────────────────────────────────────────────

void App::playTrackInternal(const TrackInfo* track) {
    if (!track) return;
    // Set pause state BEFORE loading so mpv loads into the right state
    if (m_settings.startPaused)
        m_engine.setPaused(true);
    std::string utf8Path = platform::toUtf8(track->fullPath);
    m_engine.loadFile(utf8Path);
    m_pluginHost.fireEvent(WAVE_EVENT_TRACK_CHANGED);
    // Start waveform scan — use a default duration if mpv hasn't reported one yet
    double scanDur = m_engine.duration();
    if (scanDur < 1.0) scanDur = 180.0; // assume 3 min if unknown
    m_waveformCache.scanTrack(track->fullPath, scanDur);
}

void App::playTrack(int viewIndex) {
    if (viewIndex < 0 || viewIndex >= m_library.count()) return;
    // Skip header rows
    auto* row = m_library.viewRowAt(viewIndex);
    if (!row || row->isHeader) return;
    int masterIdx = m_library.viewToMaster(viewIndex);
    if (masterIdx < 0) return;
    auto* track = m_library.trackAt(masterIdx);
    if (!track) return;
    m_library.setPlayingIndex(masterIdx);
    m_library.setSelectedIndex(viewIndex);
    m_playSource = PlaybackSource::Library;
    playTrackInternal(track);
}

void App::playPlaylistTrack(int playlistIdx, int trackIdx) {
    auto* pl = m_playlistMgr.playlist(playlistIdx);
    if (!pl || trackIdx < 0 || trackIdx >= static_cast<int>(pl->tracks.size())) return;
    pl->playingIndex = trackIdx;
    m_activePlaylistIdx = playlistIdx;
    m_playSource = PlaybackSource::Playlist;
    playTrackInternal(&pl->tracks[trackIdx]);
}

void App::playNext() {
    TrackInfo queuedTrack;
    if (m_playlistMgr.dequeueNext(queuedTrack)) {
        m_playSource = PlaybackSource::Queue;
        playTrackInternal(&queuedTrack);
        return;
    }

    if (m_playSource == PlaybackSource::Playlist) {
        auto* pl = m_playlistMgr.playlist(m_activePlaylistIdx);
        if (pl) {
            if (pl->playingIndex + 1 < static_cast<int>(pl->tracks.size())) {
                pl->playingIndex++;
                playTrackInternal(&pl->tracks[pl->playingIndex]);
                return;
            } else if (m_repeatMode == RepeatMode::All && !pl->tracks.empty()) {
                pl->playingIndex = 0;
                playTrackInternal(&pl->tracks[0]);
                return;
            }
        }
    }

    // Shuffle: pick a random track from the library
    if (m_shuffle && m_library.totalCount() > 1) {
        static std::mt19937 rng(std::random_device{}());
        // Pick a random view index that isn't a header
        int attempts = 0;
        while (attempts < 50) {
            int ri = std::uniform_int_distribution<int>(0, m_library.count() - 1)(rng);
            auto* row = m_library.viewRowAt(ri);
            if (row && !row->isHeader) {
                int mi = m_library.viewToMaster(ri);
                if (mi >= 0 && mi != m_library.playingIndex()) {
                    auto* track = m_library.trackAt(mi);
                    if (track) {
                        m_library.setPlayingIndex(mi);
                        m_playSource = PlaybackSource::Library;
                        playTrackInternal(track);
                        return;
                    }
                }
            }
            attempts++;
        }
    }

    auto* track = m_library.next();
    if (track) {
        m_playSource = PlaybackSource::Library;
        playTrackInternal(track);
    } else if (m_repeatMode == RepeatMode::All && m_library.totalCount() > 0) {
        // Wrap to first track
        auto* first = m_library.trackAt(0);
        if (first) {
            m_library.setPlayingIndex(0);
            m_playSource = PlaybackSource::Library;
            playTrackInternal(first);
        }
    }
}

void App::playPrev() {
    if (m_playSource == PlaybackSource::Playlist) {
        auto* pl = m_playlistMgr.playlist(m_activePlaylistIdx);
        if (pl && pl->playingIndex > 0) {
            pl->playingIndex--;
            playTrackInternal(&pl->tracks[pl->playingIndex]);
            return;
        }
    }

    auto* track = m_library.prev();
    if (track) {
        m_playSource = PlaybackSource::Library;
        playTrackInternal(track);
    }
}

void App::onTrackEnded() {
    m_pluginHost.fireEvent(WAVE_EVENT_PLAYBACK_STATE);

    // Repeat One: replay the same track
    if (m_repeatMode == RepeatMode::One) {
        auto* track = m_library.current();
        if (track) {
            playTrackInternal(track);
            return;
        }
    }

    playNext();
}

void App::cycleRepeat() {
    switch (m_repeatMode) {
        case RepeatMode::Off: m_repeatMode = RepeatMode::All; break;
        case RepeatMode::All: m_repeatMode = RepeatMode::One; break;
        case RepeatMode::One: m_repeatMode = RepeatMode::Off; break;
    }
    m_settings.repeatMode = static_cast<int>(m_repeatMode);
}

void App::toggleShuffle() {
    m_shuffle = !m_shuffle;
    m_settings.shuffle = m_shuffle;
}

// ── Playlist/queue management ────────────────────────────────

void App::createPlaylist() {
    int n = m_playlistMgr.playlistCount() + 1;
    std::wstring name = L"Playlist " + std::to_wstring(n);
    m_playlistMgr.createPlaylist(name);
}

void App::deletePlaylist(int index) {
    if (m_playSource == PlaybackSource::Playlist && m_activePlaylistIdx == index) {
        m_playSource = PlaybackSource::Library;
        m_activePlaylistIdx = -1;
    }
    m_playlistMgr.deletePlaylist(index);
}

void App::addTrackToPlaylist(int playlistIdx, const TrackInfo& track) {
    m_playlistMgr.addToPlaylist(playlistIdx, track);
}

void App::addTrackToQueue(const TrackInfo& track) {
    m_playlistMgr.enqueue(track);
}

void App::playCoverFlowAlbum() {
    auto* album = m_coverFlow.focusedAlbum();
    if (!album || album->trackIndices.empty()) return;

    // Play first track of the focused album
    int masterIdx = album->trackIndices[0];
    auto* track = m_library.trackAt(masterIdx);
    if (!track) return;

    m_library.setPlayingIndex(masterIdx);
    m_playSource = PlaybackSource::Library;
    playTrackInternal(track);
    log::info("CoverFlow: playing album \"" + platform::toUtf8(album->name) + "\"");
}

} // namespace wave
