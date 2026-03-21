#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace wave {

class Library;

struct CoverAlbum {
    std::wstring name;
    std::wstring artist;
    std::wstring firstTrackDir;       // directory of first track (for art lookup)
    std::vector<int> trackIndices;    // master indices into Library
};

class CoverFlow {
public:
    ~CoverFlow();

    // Rebuild album list from library (skips if unchanged)
    void rebuild(Library* library);
    void forceRebuild() { m_lastTrackCount = -1; }

    // Navigation
    void moveLeft();
    void moveRight();
    void scrollBy(int delta);

    int focusedIndex() const { return m_focusIndex; }
    int albumCount() const { return static_cast<int>(m_albums.size()); }
    const CoverAlbum* focusedAlbum() const;
    const CoverAlbum* albumAt(int index) const;

    // Animation — call each frame
    void update(float dt);
    float smoothOffset() const { return m_smoothOffset; }

    // Art cache — lazy loads on demand
    ID2D1Bitmap* getArt(int albumIndex, ID2D1RenderTarget* rt);
    ID2D1Bitmap* getArtByName(const std::wstring& albumName, ID2D1RenderTarget* rt);
    void clearArtCache();

    bool empty() const { return m_albums.empty(); }

private:
    bool loadFolderArt(const std::wstring& dir, ID2D1RenderTarget* rt, ID2D1Bitmap** out);
    bool loadImageFile(const std::wstring& path, ID2D1RenderTarget* rt, ID2D1Bitmap** out);

    std::vector<CoverAlbum> m_albums;
    int m_focusIndex = 0;
    float m_smoothOffset = 0.0f;

    std::unordered_map<int, ID2D1Bitmap*> m_artCache;
    std::unordered_map<std::wstring, int> m_nameToIndex; // album name -> index
    int m_lastTrackCount = -1;
    std::wstring m_firstTrackPath; // for change detection
};

} // namespace wave
