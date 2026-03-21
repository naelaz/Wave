#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <string>

namespace wave {

class AlbumArt {
public:
    ~AlbumArt();

    // Load art for a track. Tries embedded thumbnail first, then folder images.
    // Returns true if art was loaded (or was already cached for this path).
    bool loadForTrack(const std::wstring& trackPath, ID2D1RenderTarget* rt);

    // Clear cached art (e.g., on stop)
    void clear();

    // Current D2D bitmap, or nullptr if no art
    ID2D1Bitmap* bitmap() const { return m_bitmap; }

    // Whether we have art loaded
    bool hasArt() const { return m_bitmap != nullptr; }

    // The path we currently have art cached for
    const std::wstring& cachedPath() const { return m_cachedPath; }

private:
    bool loadEmbeddedArt(const std::wstring& trackPath, ID2D1RenderTarget* rt);
    bool loadFolderArt(const std::wstring& trackPath, ID2D1RenderTarget* rt);
    bool loadImageFile(const std::wstring& imagePath, ID2D1RenderTarget* rt);

    ID2D1Bitmap* m_bitmap = nullptr;
    std::wstring m_cachedPath;
};

} // namespace wave
