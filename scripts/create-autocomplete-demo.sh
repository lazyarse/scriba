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
printf '[General]\nemojiMode=color\n' >> /tmp/scriba-demo-config/Scriba/Scriba.conf
export XDG_CONFIG_HOME=/tmp/scriba-demo-config

# Temp empty file in project dir — gives file completion correct relative root
touch "$PROJECT_DIR/.demo-content.md"

xvfb-run -a --server-args="-screen 0 1200x800x24" bash -c '
BUILD_DIR="'"$BUILD_DIR"'"
PROJECT_DIR="'"$PROJECT_DIR"'"
FRAMES_DIR="/tmp/scriba-demo-frames"

capture() {
    import -window root "$FRAMES_DIR/raw$(printf "%02d" $1).png"
    convert "$FRAMES_DIR/raw$(printf "%02d" $1).png" -crop 800x500+0+0 +repage "$FRAMES_DIR/frame$(printf "%02d" $1).png"
    rm -f "$FRAMES_DIR/raw$(printf "%02d" $1).png"
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
# Scene 1: Emoji autocomplete — type :smi, cycle, accept
# ═══════════════════════════════════════
for CH in "colon" "s" "m"; do
    xdotool key --window "$WID" "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
# Type i and capture popup
xdotool key --window "$WID" "i"
sleep 0.4
capture $F; F=$((F + 1))

# Press Down twice to cycle through popup items
xdotool key --window "$WID" Down
sleep 0.3
capture $F; F=$((F + 1))
sleep 0.3
capture $F; F=$((F + 1))
xdotool key --window "$WID" Down
sleep 0.3
capture $F; F=$((F + 1))
sleep 0.3
capture $F; F=$((F + 1))

# Accept with Enter, then hold (3 pause frames)
xdotool key --window "$WID" Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# ═══════════════════════════════════════
# Scene 2: File autocomplete — chain resources/ → icons/ → scriba.svg
# ═══════════════════════════════════════

# Start new line
xdotool key --window "$WID" "Return"
sleep 0.2
capture $F; F=$((F + 1))

# Type ![](
xdotool key --window "$WID" "exclam" "bracketleft" "bracketright" "parenleft"
sleep 0.1
capture $F; F=$((F + 1))

# Type "resour" — popup shows resources/
for CH in "r" "e" "s" "o" "u" "r"; do
    xdotool key --window "$WID" "$CH"
    sleep 0.1
    capture $F; F=$((F + 1))
done
# Popup frame
sleep 0.4
capture $F; F=$((F + 1))

# Accept resources/
xdotool key --window "$WID" Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type "ic" — popup shows icons/
for CH in "i" "c"; do
    xdotool key --window "$WID" "$CH"
    sleep 0.15
    capture $F; F=$((F + 1))
done
# Popup frame
sleep 0.4
capture $F; F=$((F + 1))

# Accept icons/
xdotool key --window "$WID" Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

# Type "scri" — popup shows scriba.svg
for CH in "s" "c" "r"; do
    xdotool key --window "$WID" "$CH"
    sleep 0.1
    capture $F; F=$((F + 1))
done
xdotool key --window "$WID" "i"
sleep 0.4
capture $F; F=$((F + 1))

# Accept scriba.svg (auto-closes parenthesis)
xdotool key --window "$WID" Return
for i in 1 2 3; do
    sleep 0.2
    capture $F; F=$((F + 1))
done

kill $PID 2>/dev/null
wait $PID 2>/dev/null
' || true

convert -delay 20 -loop 0 "$FRAMES_DIR"/frame*.png "$OUTPUT"

rm -f "$PROJECT_DIR/.demo-content.md"

NFRAMES=$(ls -1 "$FRAMES_DIR"/*.png 2>/dev/null | wc -l)
echo "Created $OUTPUT ($NFRAMES frames)"
