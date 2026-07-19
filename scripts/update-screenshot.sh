#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

if [ ! -f "$BUILD_DIR/scriba" ]; then
    echo "Binary not found. Building first..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

cp "$PROJECT_DIR/resources/sample.md" /tmp/sample.md

xvfb-run -a sh -c '
    '"$BUILD_DIR"'/scriba /tmp/sample.md &
    PID=$!
    sleep 3
    WID=$(xdotool search --onlyvisible --name "Scriba" | head -1)
    import -window "$WID" '"$PROJECT_DIR"'/docs/images/screenshot.png
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
'

echo "Updated $PROJECT_DIR/docs/images/screenshot.png"
