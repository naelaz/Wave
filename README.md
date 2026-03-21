# Wave Audio Player

A lightweight, high-performance local music player for Windows.

Built with C++, Win32, Direct2D/DirectWrite, and libmpv.

## Features

- **Local playback** — MP3, FLAC, WAV, AAC, OGG, Opus, M4A, WMA, AIFF, and more via libmpv
- **Library browsing** — open a folder, search by title/artist/album, sort by any field
- **Playlists & queue** — create playlists, queue tracks, persistent across sessions
- **Album art** — embedded art extraction + folder art (cover.jpg, folder.jpg, etc.)
- **Real-time visualizer** — spectrum analyzer using WASAPI loopback capture + FFT
- **Waveform scrub preview** — drag up from the timeline for a waveform overview
- **Themes** — Dark, Dark Blue, Light, High Contrast + 8 accent colors
- **Layout customization** — show/hide panels, save/load layout presets
- **Native plugin SDK** — C ABI DLL plugin system with commands, events, and host API
- **Portable mode** — drop `portable.txt` next to the exe, data stays local
- **Lightweight** — ~220KB release binary, <100MB RAM idle

## Requirements

- Windows 10/11 (x64)
- [libmpv](https://sourceforge.net/projects/mpv-player-windows/files/libmpv/) — place `libmpv-2.dll` next to `Wave.exe`

## Build

Requires Visual Studio Build Tools 2022+ with C++ workload and CMake.

```
cmake -B build -A x64
cmake --build build --config Release
```

Copy `libmpv-2.dll` into `build/Release/` and run `Wave.exe`.

## Package for distribution

```bash
bash package.sh Release
```

Creates `dist/Wave/` with everything needed. Add `portable.txt` for portable mode.

## Installer

Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php):

```
iscc installer.iss
```

Produces `dist/WaveSetup-0.1.0-beta.exe`.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Space | Play / Pause |
| Left / Right | Seek -5s / +5s |
| Up / Down | Volume +1 / -1 |
| Ctrl+O | Open file |
| Ctrl+Shift+O | Open folder |
| Ctrl+F | Focus search |
| Escape | Clear search |
| Enter | Play selected track |

## Plugin SDK

Place `.dll` plugins in the `plugins/` folder next to `Wave.exe`. See `sdk/wave_plugin_sdk.h` for the API.

Plugins can:
- Register commands (appear in Plugins menu)
- Subscribe to events (track change, playback state, etc.)
- Query and control playback
- Log messages

## Data locations

| Mode | Settings | Playlists | Logs |
|------|----------|-----------|------|
| Installed | `%APPDATA%\Wave\` | `%APPDATA%\Wave\` | `%APPDATA%\Wave\wave.log` |
| Portable | `.\data\` | `.\data\` | `.\data\wave.log` |

## License

MIT
