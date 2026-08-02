#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
OUT_DIR="$PROJECT_DIR/docs/images"

if [ ! -f "$BUILD_DIR/scriba" ]; then
    echo "Binary not found. Building first..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

cp "$PROJECT_DIR/docs/kitchensink.md" /tmp/kitchensink.md

xvfb-run -a sh -c '
    '"$BUILD_DIR"'/scriba /tmp/kitchensink.md &
    PID=$!
    sleep 3
    WID=$(xdotool search --onlyvisible --name "Scriba" | head -1)
    xdotool windowsize "$WID" 1280 800
    xdotool windowfocus "$WID"
    sleep 3
    import -window "$WID" '"$OUT_DIR"'/screenshot.png

    # waitwin <title> -> echoes window id once it appears (or empty)
    waitwin() {
        for _ in $(seq 1 60); do
            W=$(xdotool search --onlyvisible --name "$1" | head -1)
            if [ -n "$W" ]; then echo "$W"; return 0; fi
            sleep 0.2
        done
        return 1
    }

    # capture <title> <outfile> [sleep]: wait for dialog, let WebEngine render, shoot
    capture() {
        local W
        W=$(waitwin "$1") || { echo "WARN: window \"$1\" not found"; return 1; }
        sleep "${3:-4}"
        import -window "$W" "$2"
        xdotool key Escape
        sleep 1
        xdotool windowfocus "$WID"
        echo "  -> $2"
    }

    # open <shortcut>: send XTEST key combo to the focused main window
    open() {
        xdotool windowfocus "$WID"
        sleep 0.2
        xdotool key "$1"
    }

    # --- Preferences: capture each page ---
    open ctrl+alt+p
    PREF=$(waitwin "Preferences") || exit 1
    sleep 1
    xdotool mousemove --window "$PREF" 60 35 click 1
    sleep 1
    import -window "$PREF" '"$OUT_DIR"'/preferences-general.png
    echo "  -> '"$OUT_DIR"'/preferences-general.png"
    for PAGE in themes editor writing spelling security; do
        xdotool key Down
        sleep 1
        import -window "$PREF" '"$OUT_DIR"'/preferences-$PAGE.png
        echo "  -> '"$OUT_DIR"'/preferences-$PAGE.png"
    done
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"

    # --- Table helper ---
    open ctrl+t
    capture "Insert Table" '"$OUT_DIR"'/table-dialog.png

    # --- Emoji picker ---
    open ctrl+e
    capture "Emoji Picker" '"$OUT_DIR"'/emoji-picker.png

    # --- KaTeX helper (type a sample equation so the preview renders) ---
    open ctrl+k
    KATEX=$(waitwin "Insert Equation") || exit 1
    sleep 1
    xdotool type -- "E = mc^2"
    sleep 4
    import -window "$KATEX" '"$OUT_DIR"'/katex-dialog.png
    echo "  -> '"$OUT_DIR"'/katex-dialog.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"

    # --- Mchem helper (type a sample formula so the preview renders) ---
    open ctrl+shift+k
    MCHEM=$(waitwin "Insert Chemistry Notation") || exit 1
    sleep 1
    xdotool type -- "2H_2 + O_2 -> 2H_2O"
    sleep 4
    import -window "$MCHEM" '"$OUT_DIR"'/mchem-dialog.png
    echo "  -> '"$OUT_DIR"'/mchem-dialog.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"

    # --- Vega-Lite chart builder (renders sample chart on open) ---
    open ctrl+g
    capture "Chart Builder" '"$OUT_DIR"'/vega-lite-dialog.png

    # --- Mermaid chart helper (default pie chart) ---
    open ctrl+m
    capture "Mermaid Chart" '"$OUT_DIR"'/mermaid-dialog.png

    # --- Print / Export PDF (long wait: WebEngine load, vega/mermaid
    # promises, chromium --print-to-pdf, then preview reload) ---
    open ctrl+p
    capture "Print / Export PDF" '"$OUT_DIR"'/print-pdf-dialog.png 12

    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
'

echo "Updated screenshots in $OUT_DIR"
