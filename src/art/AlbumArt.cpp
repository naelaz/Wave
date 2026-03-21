#include "art/AlbumArt.h"
#include "core/Log.h"

#include <wincodec.h>
#include <thumbcache.h>
#include <shlobj.h>

#pragma comment(lib, "windowscodecs.lib")

namespace wave {

AlbumArt::~AlbumArt() {
    clear();
}

void AlbumArt::clear() {
    if (m_bitmap) { m_bitmap->Release(); m_bitmap = nullptr; }
    m_cachedPath.clear();
}

// ── Main entry point ─────────────────────────────────────────

bool AlbumArt::loadForTrack(const std::wstring& trackPath, ID2D1RenderTarget* rt) {
    if (!rt) return false;

    // Already cached for this track
    if (trackPath == m_cachedPath && m_bitmap) return true;

    // Clear old
    if (m_bitmap) { m_bitmap->Release(); m_bitmap = nullptr; }
    m_cachedPath = trackPath;

    // Try embedded thumbnail first (Windows Shell extracts from audio metadata)
    if (loadEmbeddedArt(trackPath, rt)) return true;

    // Try common folder art files
    if (loadFolderArt(trackPath, rt)) return true;

    return false;
}

// ── Embedded art via Shell thumbnail ─────────────────────────
// Windows Shell can extract album art from MP3/FLAC/etc. ID3/Vorbis tags

bool AlbumArt::loadEmbeddedArt(const std::wstring& trackPath, ID2D1RenderTarget* rt) {
    IShellItem* item = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(trackPath.c_str(), nullptr, IID_PPV_ARGS(&item));
    if (FAILED(hr) || !item) return false;

    IThumbnailCache* cache = nullptr;
    hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&cache));
    if (FAILED(hr) || !cache) {
        item->Release();
        // Fallback: try IThumbnailProvider directly from the shell item
        IThumbnailProvider* prov = nullptr;
        hr = item->BindToHandler(nullptr, BHID_ThumbnailHandler, IID_PPV_ARGS(&prov));
        if (SUCCEEDED(hr) && prov) {
            HBITMAP hbm = nullptr;
            WTS_ALPHATYPE alpha;
            hr = prov->GetThumbnail(512, &hbm, &alpha);
            prov->Release();
            item->Release();
            if (SUCCEEDED(hr) && hbm) {
                // Convert HBITMAP to D2D bitmap via WIC
                IWICImagingFactory* wic = nullptr;
                CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&wic));
                if (wic) {
                    IWICBitmap* wicBmp = nullptr;
                    wic->CreateBitmapFromHBITMAP(hbm, nullptr, WICBitmapUsePremultipliedAlpha, &wicBmp);
                    if (wicBmp) {
                        IWICFormatConverter* converter = nullptr;
                        wic->CreateFormatConverter(&converter);
                        if (converter) {
                            converter->Initialize(wicBmp, GUID_WICPixelFormat32bppPBGRA,
                                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                                   WICBitmapPaletteTypeCustom);
                            rt->CreateBitmapFromWicBitmap(converter, nullptr, &m_bitmap);
                            converter->Release();
                        }
                        wicBmp->Release();
                    }
                    wic->Release();
                }
                DeleteObject(hbm);
                return m_bitmap != nullptr;
            }
            return false;
        }
        item->Release();
        return false;
    }

    // Use the thumbnail cache
    ISharedBitmap* shared = nullptr;
    WTS_CACHEFLAGS flags;
    WTS_THUMBNAILID thumbId;
    hr = cache->GetThumbnail(item, 512, WTS_EXTRACT | WTS_SCALETOREQUESTEDSIZE,
                              &shared, &flags, &thumbId);
    cache->Release();
    item->Release();

    if (FAILED(hr) || !shared) return false;

    HBITMAP hbm = nullptr;
    hr = shared->GetSharedBitmap(&hbm);
    if (FAILED(hr) || !hbm) { shared->Release(); return false; }

    // Convert HBITMAP → WIC → D2D bitmap
    IWICImagingFactory* wic = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                      IID_PPV_ARGS(&wic));
    if (wic) {
        IWICBitmap* wicBmp = nullptr;
        wic->CreateBitmapFromHBITMAP(hbm, nullptr, WICBitmapUsePremultipliedAlpha, &wicBmp);
        if (wicBmp) {
            IWICFormatConverter* converter = nullptr;
            wic->CreateFormatConverter(&converter);
            if (converter) {
                converter->Initialize(wicBmp, GUID_WICPixelFormat32bppPBGRA,
                                       WICBitmapDitherTypeNone, nullptr, 0.0,
                                       WICBitmapPaletteTypeCustom);
                rt->CreateBitmapFromWicBitmap(converter, nullptr, &m_bitmap);
                converter->Release();
            }
            wicBmp->Release();
        }
        wic->Release();
    }

    // Note: do NOT DeleteObject(hbm) — it's owned by the shared bitmap
    shared->Release();

    return m_bitmap != nullptr;
}

// ── Folder art (cover.jpg, folder.jpg, etc.) ─────────────────

bool AlbumArt::loadFolderArt(const std::wstring& trackPath, ID2D1RenderTarget* rt) {
    // Extract directory from track path
    auto slash = trackPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return false;
    std::wstring dir = trackPath.substr(0, slash + 1);

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
            if (loadImageFile(path, rt)) return true;
        }
    }
    return false;
}

// ── Load an image file via WIC ───────────────────────────────

bool AlbumArt::loadImageFile(const std::wstring& imagePath, ID2D1RenderTarget* rt) {
    IWICImagingFactory* wic = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&wic));
    if (FAILED(hr) || !wic) return false;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wic->CreateDecoderFromFilename(imagePath.c_str(), nullptr,
                                          GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) { wic->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) { decoder->Release(); wic->Release(); return false; }

    IWICFormatConverter* converter = nullptr;
    wic->CreateFormatConverter(&converter);
    if (converter) {
        converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
        rt->CreateBitmapFromWicBitmap(converter, nullptr, &m_bitmap);
        converter->Release();
    }

    frame->Release();
    decoder->Release();
    wic->Release();

    return m_bitmap != nullptr;
}

} // namespace wave
