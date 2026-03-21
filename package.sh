#!/bin/bash
# ── Wave Packaging Script ────────────────────────────────────
# Creates a clean release folder ready for distribution.
# Usage: bash package.sh [Release|Debug]
# Output: dist/Wave/

CONFIG=${1:-Release}
BUILD_DIR="build/$CONFIG"
DIST_DIR="dist/Wave"

echo "[Wave] Packaging $CONFIG build..."

if [ ! -f "$BUILD_DIR/Wave.exe" ]; then
    echo "ERROR: $BUILD_DIR/Wave.exe not found. Build first:"
    echo "  cmake -B build -A x64"
    echo "  cmake --build build --config $CONFIG"
    exit 1
fi

# Clean and create
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/plugins" "$DIST_DIR/sdk"

# Core executable
cp "$BUILD_DIR/Wave.exe" "$DIST_DIR/"

# libmpv DLL — check build dir first, then Debug dir as fallback
for dll in libmpv-2.dll mpv-2.dll; do
    [ -f "$BUILD_DIR/$dll" ] && cp "$BUILD_DIR/$dll" "$DIST_DIR/" && echo "  Copied $dll"
    [ -f "build/Debug/$dll" ] && [ ! -f "$DIST_DIR/$dll" ] && cp "build/Debug/$dll" "$DIST_DIR/" && echo "  Copied $dll from Debug"
done

if [ ! -f "$DIST_DIR/libmpv-2.dll" ] && [ ! -f "$DIST_DIR/mpv-2.dll" ]; then
    echo "  WARNING: No libmpv DLL found! Audio will not work."
    echo "  Copy libmpv-2.dll into $DIST_DIR/ manually."
fi

# Plugins
cp "$BUILD_DIR/plugins/"*.dll "$DIST_DIR/plugins/" 2>/dev/null

# SDK
cp sdk/wave_plugin_sdk.h "$DIST_DIR/sdk/"

# Portable marker
echo "Portable mode: settings stored in ./data/" > "$DIST_DIR/portable.txt"

echo ""
echo "[Wave] Package complete: $DIST_DIR/"
ls -la "$DIST_DIR/"
echo ""
du -sh "$DIST_DIR/"
echo ""
echo "To run: $DIST_DIR/Wave.exe"
echo "To make installed (non-portable): delete $DIST_DIR/portable.txt"
