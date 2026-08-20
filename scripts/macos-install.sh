#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build"
APP_NAME="ShowJCR.app"
INSTALL_PATH="/Applications/$APP_NAME"

BUILD_ONLY=false
DO_INSTALL=true
DO_OPEN=false

usage() {
    cat <<'EOF'
Usage: scripts/macos-install.sh [options]

Build ShowJCR.app, bundle Qt dependencies, sign, and optionally install.

Options:
  --build-only   Build into build/ShowJCR.app only (no install)
  --no-install   Same as --build-only
  --open         Open the app after build/install
  -h, --help     Show this help

Examples:
  ./scripts/macos-install.sh
  ./scripts/macos-install.sh --build-only --open
  make install
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only|--no-install)
            BUILD_ONLY=true
            DO_INSTALL=false
            ;;
        --open)
            DO_OPEN=true
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
    shift
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found. Install CMake first." >&2
    exit 1
fi

if command -v brew >/dev/null 2>&1; then
    QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
else
    QT_PREFIX=""
fi

if [[ -z "$QT_PREFIX" || ! -d "$QT_PREFIX/lib/cmake/Qt6" ]]; then
    echo "Qt 6 not found. Install with: brew install qt" >&2
    exit 1
fi

MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "macdeployqt not found: $MACDEPLOYQT" >&2
    exit 1
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> Configure"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PREFIX"

ICON_SRC="$ROOT/resources/image/jcr-logo.jpg"
ICON_ICNS="$ROOT/resources/appicon.icns"
if [[ ! -f "$ICON_ICNS" || "$ICON_SRC" -nt "$ICON_ICNS" ]]; then
    echo "==> Generate appicon.icns"
    ICONSET="$(mktemp -d)/ShowJCR.iconset"
    mkdir -p "$ICONSET"
    sips -s format png "$ICON_SRC" --out /tmp/showjcr-logo-src.png >/dev/null
    for size in 16 32 128 256 512; do
        sips -z "$size" "$size" /tmp/showjcr-logo-src.png --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
        sips -z $((size * 2)) $((size * 2)) /tmp/showjcr-logo-src.png --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$ICONSET" -o "$ICON_ICNS"
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PREFIX"
fi

echo "==> Build"
cmake --build "$BUILD_DIR" -j"$JOBS"

if [[ ! -d "$BUILD_DIR/$APP_NAME" ]]; then
    echo "Build failed: $BUILD_DIR/$APP_NAME not found" >&2
    exit 1
fi

echo "==> Bundle Qt (macdeployqt)"
rm -f "$BUILD_DIR/$APP_NAME/Contents/MacOS/jcr.db"
"$MACDEPLOYQT" "$BUILD_DIR/$APP_NAME"

echo "==> Sign"
codesign --force --deep --sign - "$BUILD_DIR/$APP_NAME"

if $DO_INSTALL; then
    echo "==> Install to $INSTALL_PATH"
    rm -rf "$INSTALL_PATH"
    cp -R "$BUILD_DIR/$APP_NAME" "$INSTALL_PATH"
    echo "Installed: $INSTALL_PATH"
    APP_TO_OPEN="$INSTALL_PATH"
else
    echo "Built: $BUILD_DIR/$APP_NAME"
    APP_TO_OPEN="$BUILD_DIR/$APP_NAME"
fi

if $DO_OPEN; then
    open "$APP_TO_OPEN"
fi
