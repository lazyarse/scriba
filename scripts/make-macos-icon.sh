#!/bin/bash
# Generate resources/icons/scriba.icns from resources/icons/scriba.svg.
# macOS-only: uses qlmanage (QuickLook thumbnail) + sips + iconutil.
# Run from the repository root (CI calls `bash scripts/make-macos-icon.sh`).
set -e

SRC="resources/icons/scriba.svg"
DEST="resources/icons/scriba.icns"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Rasterize the SVG to a 1024x1024 PNG thumbnail.
PNG="$TMP/$(basename "$SRC").png"
if ! qlmanage -t -s 1024 -o "$TMP" "$SRC" >/dev/null 2>&1 || [ ! -f "$PNG" ]; then
    echo "qlmanage failed to rasterize $SRC" >&2
    exit 1
fi

ICONSET="$TMP/scriba.iconset"
mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$PNG" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    sips -z "$((size * 2))" "$((size * 2))" "$PNG" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done

iconutil -c icns "$ICONSET" -o "$DEST"
echo "Wrote $DEST"
