#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
OUTPUT="$PROJECT_DIR/docs/images/autocomplete-demo.gif"
FRAMES_DIR="/tmp/scriba-demo-frames"

rm -rf "$FRAMES_DIR"
mkdir -p "$FRAMES_DIR"

if [ ! -f "$BUILD_DIR/scriba" ]; then
    echo "Building first..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

# Isolate config so the demo doesn't touch user's real QSettings
rm -rf /tmp/scriba-demo-config
mkdir -p /tmp/scriba-demo-config/Scriba
# Prefer color emoji rendering in preview
printf '[General]\nemojiMode=color\nactiveCssFile=:/themes/catppuccin-mocha.css\nfirstRun=false\n' >> /tmp/scriba-demo-config/Scriba/Scriba.conf
export XDG_CONFIG_HOME=/tmp/scriba-demo-config

# Creates an animated GIF demo of scriba's autocomplete features (emoji, file path, table).
#
# How it works:
#   xvfb-run launches scriba in a virtual framebuffer, xdotool simulates keystrokes,
#   and `import` (ImageMagick) captures frames after each key press.
#
# Key overlay:
#   Every capture() call stamps the last pressed key in a rounded badge at the
#   bottom-right corner.  Keys are forwarded through press() which sets $LAST_KEY
#   so capture() knows what to overlay.  $LAST_KEY is cleared after each capture,
#   so pause frames (no key since last capture) show no overlay.
#
# Adding a new scene:
#   1. Use press() instead of xdotool directly — it sets $LAST_KEY automatically.
#   2. Call capture $F; F=$((F + 1)) after each key press or visual change.
#   3. Use for-loops for character-by-character typing with per-frame captures.
#   4. Use "for i in 1 2 3; do sleep 0.2; capture $F; F=$((F+1)); done" for pauses.
#   5. Frame numbers use %03d (3 digits). If more than 999 frames are ever needed,
#      bump to %04d everywhere.
#   6. The last capture before kill $PID should end with a pause so the viewer
#      can see the final state.
#
# Key-name mapping (xdotool → display):
#   The case/esac block inside capture() maps xdotool key names like "Return"
#   to display-friendly labels like "Enter".  Add new mappings there if needed.
#
# To rebuild the GIF, run this script from the project root:
#   bash scripts/create-autocomplete-demo.sh

# Temp empty file in project dir — gives file completion correct relative root
touch "$PROJECT_DIR/.demo-content.md"

xvfb-run -a --server-args="-screen 0 1200x800x24" bash -c '
BUILD_DIR="'"$BUILD_DIR"'"
PROJECT_DIR="'"$PROJECT_DIR"'"
FRAMES_DIR="/tmp/scriba-demo-frames"

LAST_KEY=""

press() {
    LAST_KEY="$1"
    xdotool key --window "$WID" "$@"
}

capture() {
    import -window root "$FRAMES_DIR/raw$(printf "%03d" $1).png"
    convert "$FRAMES_DIR/raw$(printf "%03d" $1).png" -crop 800x500+0+0 +repage "$FRAMES_DIR/frame$(printf "%03d" $1).png"
    rm -f "$FRAMES_DIR/raw$(printf "%03d" $1).png"

    if [ -n "$LAST_KEY" ]; then
        local dk="$LAST_KEY"
        case "$dk" in
            "Return") dk="Enter" ;; "Down") dk="↓" ;; "Up") dk="↑" ;;
            "Tab") dk="Tab" ;; "Right") dk="→" ;; "Left") dk="←" ;;
            "BackSpace") dk="⌫" ;;
            "colon") dk=":" ;; "exclam") dk="!" ;;
            "bracketleft") dk="[" ;; "bracketright") dk="]" ;;
            "parenleft") dk="(" ;; "parenright") dk=")" ;;
            "bar") dk="|" ;; "numbersign") dk="#" ;;
            "period") dk="." ;; "slash") dk="/" ;;
            "space") dk="␣" ;; "minus") dk="-" ;;
            "ctrl+t") dk="Ctrl+T" ;; "alt+i") dk="Alt+I" ;;
        esac
        convert -background "#e8e8e8" -fill "#222222" \
            -font "DejaVu-Sans-Bold" -pointsize 75 -gravity center \
            label:" $dk " \
            -bordercolor "#e8e8e8" -border 12x6 \
            -shave 3x3 -bordercolor "#555555" -border 3 \
            "$FRAMES_DIR/tag.png"
        local tw=$(identify -format "%w" "$FRAMES_DIR/tag.png")
        local th=$(identify -format "%h" "$FRAMES_DIR/tag.png")
        rm -f "$FRAMES_DIR/tag.png"
        convert -size ${tw}x${th} xc:none \
            -fill "#e8e8e8" -stroke "#aaaaaa" -strokewidth 2 \
            -draw "roundrectangle 3,3 $((tw-4)),$((th-4)) 14,14" \
            -fill "#222222" -font "DejaVu-Sans-Bold" -pointsize 75 -gravity center \
            -annotate +0+0 " $dk " \
            \( +clone -background black -shadow 50x5+0+2 \) \
            +swap -composite \
            "$FRAMES_DIR/tag_comp.png"
        convert "$FRAMES_DIR/frame$(printf "%03d" $1).png" \
            "$FRAMES_DIR/tag_comp.png" \
            -gravity southeast -geometry +15+15 \
            -compose Over -composite \
            "$FRAMES_DIR/frame$(printf "%03d" $1).png"
    fi
}

pause_frames() {
    local count=$1
    local start=$2
    for i in $(seq 1 $count); do
        sleep 0.2
        capture $((start + i - 1))
    done
}

"$BUILD_DIR/scriba" "$PROJECT_DIR/.demo-content.md" &
PID=$!
sleep 2

WID=$(xdotool search --onlyvisible --name "Scriba" | head -1)
xdotool windowsize "$WID" 800 500
xdotool windowmove "$WID" 0 0
sleep 0.3

xdotool mousemove --window "$WID" 100 100
xdotool click 1
sleep 0.2

F=1

# ═══════════════════════════════════════
# Scene 1: File autocomplete — chain resources/ → icons/ → scriba.svg
# ═══════════════════════════════════════

# Type ![](
for CH in "exclam" "bracketleft" "bracketright" "parenleft"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

# Type "resour" — popup shows resources/
for CH in "r" "e" "s" "o" "u" "r"; do
    press "$CH"
    sleep 0.1
    capture $F; F=$((F + 1))
done
# Popup frame
sleep 0.4
capture $F; F=$((F + 1))

# Accept resources/
press Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type "ic" — popup shows icons/
for CH in "i" "c"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
# Popup frame
sleep 0.4
capture $F; F=$((F + 1))

# Accept icons/
press Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type "scri" — popup shows scriba.svg
for CH in "s" "c" "r"; do
    press "$CH"
    sleep 0.1
    capture $F; F=$((F + 1))
done
press "i"
sleep 0.4
capture $F; F=$((F + 1))

# Accept scriba.svg (auto-closes parenthesis)
press Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type #80x for image width constraint
press "Left"
sleep 0.15
capture $F; F=$((F + 1))
for CH in "numbersign" "8" "0" "x"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Right"
sleep 0.15
capture $F; F=$((F + 1))

# ═══════════════════════════════════════
# Scene 2: Emoji autocomplete — type :smi, cycle, accept
# ═══════════════════════════════════════
for CH in "colon" "s" "m"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
# Type i and capture popup
press "i"
sleep 0.4
capture $F; F=$((F + 1))

# Press Down twice to cycle through popup items
press Down
sleep 0.3
capture $F; F=$((F + 1))
sleep 0.3
capture $F; F=$((F + 1))
press Down
sleep 0.3
capture $F; F=$((F + 1))
sleep 0.3
capture $F; F=$((F + 1))

# Accept with Enter, then hold (3 pause frames)
press Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Extra blank line before table scene
press "Return"
for i in 1 2; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# ═══════════════════════════════════════
# Scene 3: Table autocomplete — type header, fill cells, exit
# ═══════════════════════════════════════

# Start new line (after previous content)
press "Return"
sleep 0.2
capture $F; F=$((F + 1))
press "Return"
sleep 0.2
capture $F; F=$((F + 1))

# Type |header1|
for CH in "bar" "h" "e" "a" "d" "e" "r" "1" "bar"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

# Type header2|
for CH in "h" "e" "a" "d" "e" "r" "2" "bar"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

# Type header3|
for CH in "h" "e" "a" "d" "e" "r" "3" "bar"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

# Press Enter — table autocomplete inserts separator + data row
press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Fill row 1: cell1 → Tab → cell2 → Tab → cell3 → Enter
for CH in "c" "e" "l" "l" "1"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))
for CH in "c" "e" "l" "l" "2"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))
for CH in "c" "e" "l" "l" "3"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Fill row 2: cell4 → Tab → cell5 → Tab → cell6 → Enter (creates blank row)
for CH in "c" "e" "l" "l" "4"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))
for CH in "c" "e" "l" "l" "5"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))
for CH in "c" "e" "l" "l" "6"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
# Enter creates blank row
press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Exit table: Enter on blank row clears and exits
press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done
# --- Scene 4: HTML table via Ctrl+T dialog ---
press "Return"
for i in 1 2; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

press "ctrl+t"
sleep 0.5
capture $F; F=$((F + 1))

DLG_WID=$(xdotool search --name "Insert Table" 2>/dev/null | tail -1)

LAST_KEY="2"; xdotool key --window "$DLG_WID" "2"
sleep 0.15
capture $F; F=$((F + 1))

LAST_KEY="Tab"; xdotool key --window "$DLG_WID" "Tab"
sleep 0.15
capture $F; F=$((F + 1))

LAST_KEY="space"; xdotool key --window "$DLG_WID" "space"
sleep 0.15
capture $F; F=$((F + 1))

for i in 1 2; do
    sleep 0.3
    capture $F; F=$((F + 1))
done

LAST_KEY="Alt+I"; xdotool key --window "$DLG_WID" "alt+i"
sleep 0.5
capture $F; F=$((F + 1))

# Type foo, Tab, bar, Enter in the HTML table
for CH in "f" "o" "o"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))

for CH in "b" "a" "r"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type foo2, Tab, bar2, Enter, Enter
for CH in "f" "o" "o" "2"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

press "Tab"
sleep 0.2
capture $F; F=$((F + 1))
sleep 0.2
capture $F; F=$((F + 1))

for CH in "b" "a" "r" "2"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Exit table: Enter on blank row
press "Return"
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type "End of auto-complete demo."
for CH in "E" "n" "d" "space" "o" "f" "space" "a" "u" "t" "o" "minus" "c" "o" "m" "p" "l" "e" "t" "e" "space" "d" "e" "m" "o" "period"; do
    press "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done

kill $PID 2>/dev/null
wait $PID 2>/dev/null
' || true

convert -delay 30 -loop 0 "$FRAMES_DIR"/frame*.png "$OUTPUT"

rm -f "$PROJECT_DIR/.demo-content.md"

NFRAMES=$(ls -1 "$FRAMES_DIR"/*.png 2>/dev/null | wc -l)
echo "Created $OUTPUT ($NFRAMES frames)"
