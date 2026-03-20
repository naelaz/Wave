#include "app/App.h"
#include "core/Log.h"
#include "platform/Win32Helpers.h"

#include <shellapi.h>
#include <shobjidl.h>
#include <commdlg.h>

namespace wave {

bool App::init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    log::init();
    log::info("Wave v0.1.0 starting");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!m_mainWindow.create(hInstance, this)) {
        log::error("Failed to initialize main window");
        return false;
    }

    if (!m_renderer.init(m_mainWindow.handle(), &m_engine, &m_library, &m_playlistMgr)) {
        log::error("Failed to initialize renderer");
        return false;
    }
    m_mainWindow.setRenderer(&m_renderer);

    if (!m_engine.init(m_mainWindow.handle())) {
        log::warn("Audio engine not available (is libmpv-2.dll next to Wave.exe?)");
    }

    m_engine.setTrackEndCallback([](void* ctx) {
        static_cast<App*>(ctx)->onTrackEnded();
    }, this);

    // Load saved playlists
    m_playlistMgr.load();

    // Load file/folder from command-line argument
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1) {
            std::wstring wpath = argv[1];
            DWORD attr = GetFileAttributesW(wpath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                m_library.scanFolder(wpath);
                if (!m_library.empty()) playTrack(0);
            } else {
                m_library.addFile(wpath);
                playTrack(0);
            }
        }
        LocalFree(argv);
    }

    log::info("Initialization complete");
    return true;
}

int App::run() {
    m_mainWindow.show();
    return platform::runMessageLoop();
}

void App::shutdown() {
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
                m_playSource = PlaybackSource::Library;
                if (!m_library.empty()) playTrack(0);
                CoTaskMemFree(path);
            }
            psi->Release();
        }
    }
    pfd->Release();
}

// ── Playback ─────────────────────────────────────────────────

void App::playTrackInternal(const TrackInfo* track) {
    if (!track) return;
    std::string utf8Path = platform::toUtf8(track->fullPath);
    m_engine.loadFile(utf8Path);
}

void App::playTrack(int index) {
    auto* track = m_library.trackAt(index);
    if (!track) return;
    m_library.setPlayingIndex(index);
    m_library.setSelectedIndex(index);
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
    // Queue takes priority
    TrackInfo queuedTrack;
    if (m_playlistMgr.dequeueNext(queuedTrack)) {
        m_playSource = PlaybackSource::Queue;
        playTrackInternal(&queuedTrack);
        return;
    }

    // Follow current source
    if (m_playSource == PlaybackSource::Playlist) {
        auto* pl = m_playlistMgr.playlist(m_activePlaylistIdx);
        if (pl && pl->playingIndex + 1 < static_cast<int>(pl->tracks.size())) {
            pl->playingIndex++;
            playTrackInternal(&pl->tracks[pl->playingIndex]);
            return;
        }
    }

    // Library fallback
    auto* track = m_library.next();
    if (track) {
        m_playSource = PlaybackSource::Library;
        playTrackInternal(track);
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
    playNext();
}

// ── Playlist/queue management ────────────────────────────────

void App::createPlaylist() {
    // Auto-name: "Playlist 1", "Playlist 2", etc.
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

} // namespace wave
