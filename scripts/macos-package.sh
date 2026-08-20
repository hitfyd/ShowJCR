#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST_DIR="$ROOT/dist"
APP_NAME="ShowJCR.app"
APP_PATH="$ROOT/build/$APP_NAME"

read_version() {
    sed -n 's/.*ShowJCR::version = "\(v[^"]*\)".*/\1/p' "$ROOT/showjcr.cpp" | head -n 1
}

VERSION="$(read_version)"
if [[ -z "$VERSION" ]]; then
    echo "Failed to read version from showjcr.cpp" >&2
    exit 1
fi

ARCHIVE_NAME="ShowJCR-${VERSION}-macos.zip"

echo "==> Build app bundle"
"$ROOT/scripts/macos-install.sh" --build-only

if [[ ! -d "$APP_PATH" ]]; then
    echo "App bundle not found: $APP_PATH" >&2
    exit 1
fi

mkdir -p "$DIST_DIR"
rm -f "$DIST_DIR/$ARCHIVE_NAME"

echo "==> Create archive"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$DIST_DIR/$ARCHIVE_NAME"

echo "Package ready: $DIST_DIR/$ARCHIVE_NAME"
