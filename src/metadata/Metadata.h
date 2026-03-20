#pragma once

#include <string>

namespace wave {

struct TrackMeta {
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    int trackNumber = 0;
};

// Read metadata from an audio file using the Windows Shell property system.
// Returns true if any metadata was found. Fields not found are left empty.
bool readMetadata(const std::wstring& filePath, TrackMeta& out);

} // namespace wave
