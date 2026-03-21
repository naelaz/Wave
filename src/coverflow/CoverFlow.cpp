#include "coverflow/CoverFlow.h"
#include "library/Library.h"
#include "core/Log.h"

#include <wincodec.h>
#include <algorithm>
#include <map>
#include <cmath>

#pragma comment(lib, "windowscodecs.lib")

namespace wave {

CoverFlow::~CoverFlow() {
    clearArtCache();
}

void CoverFlow::clearArtCache() {
    for (auto& [idx, bmp] : m_artCache) {
        if (bmp) bmp->Release();
    }
    m_artCache.clear();
}

// ── Album grouping ──────────────────────────────────────────

void CoverFlow::rebuild(Library* library) {
    if (!library) return;
    int count = library->totalCount();
    // Check if library actually changed: compare count + first track path
    if (count == m_lastTrackCount && count > 0 && !m_albums.empty()) {
        auto* first = library->trackAt(0);
        if (first && first->fullPath == m_firstTrackPath) return; // truly unchanged
    }
    if (count == 0) { m_albums.clear(); m_nameToIndex.clear(); m_lastTrackCount = 0; m_firstTrackPath.clear(); return; }

    clearArtCache();
    m_albums.clear();
    m_nameToIndex.clear();
    m_lastTrackCount = count;

    // Group tracks by album name (case-insensitive)
    std::map<std::wstring, int> albumMap;

    for (int i = 0; i < count; i++) {
        auto* track = library->trackAt(i);
        if (!track) continue;

        // Key: album name, or directory path for unknown albums
        std::wstring key = track->album;
        if (key.empty()) {
            auto slash = track->fullPath.find_last_of(L"\\/");
            key = (slash != std::wstring::npos) ? track->fullPath.substr(0, slash) : L"Unknown";
        }
        std::wstring lowerKey = key;
        for (auto& c : lowerKey) c = towlower(c);

        auto it = albumMap.find(lowerKey);
        if (it != albumMap.end()) {
            m_albums[it->second].trackIndices.push_back(i);
        } else {
            CoverAlbum album;
            album.name = track->album.empty() ? L"Unknown Album" : track->album;
            album.artist = track->artist.empty() ? L"Unknown Artist" : track->artist;

            auto slash = track->fullPath.find_last_of(L"\\/");
            album.firstTrackDir = (slash != std::wstring::npos) ?
                track->fullPath.substr(0, slash + 1) : L"";

            album.trackIndices.push_back(i);
            int idx = static_cast<int>(m_albums.size());
            albumMap[lowerKey] = idx;
            m_nameToIndex[album.name] = idx;
            m_albums.push_back(std::move(album));
        }
    }

    // Clamp focus
    if (m_focusIndex >= static_cast<int>(m_albums.size()))
        m_focusIndex = m_albums.empty() ? 0 : static_cast<int>(m_albums.size()) - 1;
    m_smoothOffset = static_cast<float>(m_focusIndex);

    // Remember first track for change detection
    auto* firstTrack = library->trackAt(0);
    m_firstTrackPath = firstTrack ? firstTrack->fullPath : L"";

    log::info("CoverFlow: " + std::to_string(m_albums.size()) + " albums from " +
              std::to_string(count) + " tracks");
}

// ── Navigation ──────────────────────────────────────────────

void CoverFlow::moveLeft() {
    if (m_focusIndex > 0) m_focusIndex--;
}

void CoverFlow::moveRight() {
    if (m_focusIndex < static_cast<int>(m_albums.size()) - 1) m_focusIndex++;
}

void CoverFlow::scrollBy(int delta) {
    int newIdx = m_focusIndex + delta;
    m_focusIndex = std::clamp(newIdx, 0, std::max(0, static_cast<int>(m_albums.size()) - 1));
}

const CoverAlbum* CoverFlow::focusedAlbum() const {
    if (m_focusIndex < 0 || m_focusIndex >= static_cast<int>(m_albums.size())) return nullptr;
    return &m_albums[m_focusIndex];
}

const CoverAlbum* CoverFlow::albumAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_albums.size())) return nullptr;
    return &m_albums[index];
}

// ── Smooth scroll ───────────────────────────────────────────

void CoverFlow::update(float dt) {
    float target = static_cast<float>(m_focusIndex);
    float diff = target - m_smoothOffset;
    if (std::abs(diff) < 0.001f) {
        m_smoothOffset = target;
    } else {
        // Smooth lerp — fast but fluid
        m_smoothOffset += diff * std::min(dt * 12.0f, 1.0f);
    }
}

// ── Art loading (folder art via WIC) ────────────────────────

ID2D1Bitmap* CoverFlow::getArt(int albumIndex, ID2D1RenderTarget* rt) {
    auto it = m_artCache.find(albumIndex);
    if (it != m_artCache.end()) return it->second; // may be nullptr (no art found)

    if (albumIndex < 0 || albumIndex >= static_cast<int>(m_albums.size())) return nullptr;

    ID2D1Bitmap* bmp = nullptr;
    loadFolderArt(m_albums[albumIndex].firstTrackDir, rt, &bmp);
    m_artCache[albumIndex] = bmp; // cache even if nullptr (avoid re-trying)
    return bmp;
}

ID2D1Bitmap* CoverFlow::getArtByName(const std::wstring& albumName, ID2D1RenderTarget* rt) {
    auto it = m_nameToIndex.find(albumName);
    if (it == m_nameToIndex.end()) return nullptr;
    return getArt(it->second, rt);
}

bool CoverFlow::loadFolderArt(const std::wstring& dir, ID2D1RenderTarget* rt, ID2D1Bitmap** out) {
    if (dir.empty()) return false;

    static const wchar_t* candidates[] = {
        L"cover.jpg", L"cover.png", L"Cover.jpg", L"Cover.png",
        L"folder.jpg", L"folder.png", L"Folder.jpg", L"Folder.png",
        L"front.jpg", L"front.png", L"Front.jpg", L"Front.png",
        L"album.jpg", L"album.png", L"Album.jpg", L"Album.png",
        L"artwork.jpg", L"artwork.png",
    };

    for (auto* name : candidates) {
        std::wstring path = dir + name;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            if (loadImageFile(path, rt, out)) return true;
        }
    }
    return false;
}

bool CoverFlow::loadImageFile(const std::wstring& path, ID2D1RenderTarget* rt, ID2D1Bitmap** out) {
    IWICImagingFactory* wic = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&wic));
    if (FAILED(hr) || !wic) return false;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wic->CreateDecoderFromFilename(path.c_str(), nullptr,
                                          GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) { wic->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) { decoder->Release(); wic->Release(); return false; }

    // Scale to 256x256 thumbnail for performance
    IWICBitmapScaler* scaler = nullptr;
    wic->CreateBitmapScaler(&scaler);
    if (scaler) {
        scaler->Initialize(frame, 256, 256, WICBitmapInterpolationModeFant);

        IWICFormatConverter* converter = nullptr;
        wic->CreateFormatConverter(&converter);
        if (converter) {
            converter->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
            rt->CreateBitmapFromWicBitmap(converter, nullptr, out);
            converter->Release();
        }
        scaler->Release();
    }

    frame->Release();
    decoder->Release();
    wic->Release();

    return *out != nullptr;
}

} // namespace wave
