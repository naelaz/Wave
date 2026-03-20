#include "metadata/Metadata.h"

#include <Windows.h>
#include <shlobj.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>

#pragma comment(lib, "propsys.lib")

namespace wave {

static std::wstring getPropString(IPropertyStore* ps, const PROPERTYKEY& key) {
    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (SUCCEEDED(ps->GetValue(key, &pv))) {
        wchar_t* str = nullptr;
        if (SUCCEEDED(PropVariantToStringAlloc(pv, &str)) && str) {
            std::wstring result = str;
            CoTaskMemFree(str);
            PropVariantClear(&pv);
            return result;
        }
        PropVariantClear(&pv);
    }
    return {};
}

static int getPropUInt(IPropertyStore* ps, const PROPERTYKEY& key) {
    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (SUCCEEDED(ps->GetValue(key, &pv))) {
        ULONG val = 0;
        if (SUCCEEDED(PropVariantToUInt32(pv, &val))) {
            PropVariantClear(&pv);
            return static_cast<int>(val);
        }
        PropVariantClear(&pv);
    }
    return 0;
}

bool readMetadata(const std::wstring& filePath, TrackMeta& out) {
    IPropertyStore* ps = nullptr;
    HRESULT hr = SHGetPropertyStoreFromParsingName(
        filePath.c_str(), nullptr, GPS_READWRITE, IID_PPV_ARGS(&ps));

    // GPS_READWRITE may fail for read-only files; try GPS_DEFAULT
    if (FAILED(hr)) {
        hr = SHGetPropertyStoreFromParsingName(
            filePath.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS(&ps));
    }
    if (FAILED(hr) || !ps) return false;

    out.title       = getPropString(ps, PKEY_Title);
    out.artist      = getPropString(ps, PKEY_Music_Artist);
    out.album       = getPropString(ps, PKEY_Music_AlbumTitle);
    out.trackNumber = getPropUInt(ps, PKEY_Music_TrackNumber);

    ps->Release();

    return !out.title.empty() || !out.artist.empty() || !out.album.empty();
}

} // namespace wave
