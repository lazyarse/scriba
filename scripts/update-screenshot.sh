#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
OUT_DIR="$PROJECT_DIR/docs/images"

# Sidebar page names of the Preferences dialog, top to bottom. The first page
# is selected by clicking the list; each Down moves to the next page (see
# shot_preference_page). When adding a new page, extend this list AND the
# idx mapping in shot_preference_page.
PREF_PAGES=(general themes editor preview printing advanced writing typography replacements spelling corpus security)

# Table of available targets: <argument> -> "<shot function>|<one-line help>".
# This is the single source of truth for --help, argument validation, the full
# suite and the dispatch inside the xvfb child. To add a new screenshot, write
# a shot_<name>() function in the child and add ONE line here; everything else
# picks it up automatically. (Values must not contain '=' or newlines, and no
# target name may collide with a PREF_PAGES entry.)
declare -A TARGETS=(
    [screenshot]="shot_main|main window (docs/kitchensink.md loaded)"
    [tabbar]="shot_tabbar|tab bar with three open tabs"
    [gutter-pencil]="shot_gutter_pencil|small crop: chart-edit pencil beside the mermaid fence"
    [preferences]="shot_preferences|all Preferences dialog pages (general ... security)"
    [table-dialog]="shot_table_dialog|Insert Table helper"
    [emoji-picker]="shot_emoji_picker|Emoji picker"
    [katex-dialog]="shot_katex_dialog|Insert Equation helper"
    [mchem-dialog]="shot_mchem_dialog|Insert Chemistry Notation helper"
    [chart-dialog]="shot_chart_dialog|Chart builder (ECharts)"
    [stock-chart-dialog]="shot_stock_chart_dialog|Stock chart builder"
    [advanced-charts]="shot_advanced_charts|Advanced Charts builder (sankey/treemap/...)"
    [mermaid-dialog]="shot_mermaid_dialog|Mermaid chart helper"
    [mermaid-gitgraph]="shot_mermaid_gitgraph|Mermaid Git Graph helper (loads the scriba repo)"
    [check-spelling]="shot_check_spelling|Check Spelling dialog"
    [validation-report]="shot_validation_report|Validation Report options"
    [toc]="shot_toc|Table of Contents tab with a fixture corpus open"
    [print-pdf-dialog]="shot_print_pdf_dialog|Print / Export PDF dialog"
)

# Display order for --help rows and the full-suite run (assoc arrays don't
# preserve insertion order).
TARGET_ORDER=(screenshot tabbar gutter-pencil preferences table-dialog emoji-picker katex-dialog
              mchem-dialog chart-dialog stock-chart-dialog advanced-charts mermaid-dialog mermaid-gitgraph
              check-spelling validation-report toc print-pdf-dialog)

usage() {
    cat <<'EOF'
Usage: scripts/update-screenshot.sh [target...]

Capture app screenshots into docs/images/ under Xvfb. With no arguments every
screenshot is captured (full suite). With one or more targets only the named
shots are regenerated -- which is much faster for a single UI change.

Targets:
EOF
    local t w=0
    for t in "${TARGET_ORDER[@]}" "preferences-<page>"; do
        ((${#t} > w)) && w=${#t}
    done
    printf '  %-*s  %s\n' "$w" "preferences-<page>" "one Preferences page; <page> one of:"
    printf '  %-*s  %s\n' "$w" "" "general themes editor preview advanced"
    printf '  %-*s  %s\n' "$w" "" "writing typography replacements spelling security"
    printf '  %-*s  %s\n' "$w" "" "(a bare <page> name also works, e.g. \"spelling\")"
    for t in "${TARGET_ORDER[@]}"; do
        printf '  %-*s  %s\n' "$w" "$t" "${TARGETS[$t]#*|}"
    done
    cat <<'EOF'

Examples:
  scripts/update-screenshot.sh                        # full suite
  scripts/update-screenshot.sh preferences-spelling   # one Preferences page
  scripts/update-screenshot.sh spelling               # same, bare page name
  scripts/update-screenshot.sh table-dialog katex-dialog

A pre-built Release binary at build/scriba is expected; the script builds one
if it is missing.
EOF
}

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
    exit 0
fi

if [ ! -f "$BUILD_DIR/scriba" ]; then
    echo "Binary not found. Building first..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

cp "$PROJECT_DIR/docs/kitchensink.md" /tmp/kitchensink.md

# No args -> full suite.
if [ "$#" -eq 0 ]; then
    set -- all
fi

# Validate targets before launching the app, so a typo fails fast instead of
# after the slow WebEngine/app startup. Allowed set = TARGETS keys plus the
# PREF_PAGES page names (as preferences-<page> or bare).
PREF_PAGES_STR=" ${PREF_PAGES[*]} "
page_valid() { case "$PREF_PAGES_STR" in *" $1 "*) return 0 ;; *) return 1 ;; esac; }
for arg in "$@"; do
    case "$arg" in
        all)
            ;;
        preferences-*)
            page_valid "${arg#preferences-}" || { echo "Unknown target: $arg" >&2; usage; exit 1; }
            ;;
        *)
            if [[ -z "${TARGETS[$arg]+x}" ]] && ! page_valid "$arg"; then
                echo "Unknown target: $arg" >&2; usage; exit 1
            fi
            ;;
    esac
done

# Serialize the TARGETS table for the xvfb child (assoc arrays can't be
# exported directly). Values are "fn|description" and contain no '=' or
# newlines, so "key=value" lines round-trip cleanly.
SCRIBA_TARGETS=""
for t in "${!TARGETS[@]}"; do
    SCRIBA_TARGETS+="$t=${TARGETS[$t]}"$'\n'
done
export SCRIBA_TARGETS
export SCRIBA_TARGET_ORDER="${TARGET_ORDER[*]}"
export SCRIBA_OUT_DIR="$OUT_DIR"
export SCRIBA_BUILD_DIR="$BUILD_DIR"
export SCRIBA_PREF_PAGES="${PREF_PAGES[*]}"

xvfb-run -a bash -s -- "$@" <<'_SCRIBASH_'
OUT_DIR="${SCRIBA_OUT_DIR:?}"
BUILD_DIR="${SCRIBA_BUILD_DIR:?}"
PREF_PAGES=(${SCRIBA_PREF_PAGES})            # array of all Preferences pages, in sidebar order
TARGET_ORDER=(${SCRIBA_TARGET_ORDER})        # full-suite / --help order
declare -A TARGETS=()                        # target -> "shot fn|help text" (see top of file)
while IFS='=' read -r k v; do
    [[ -n "$k" ]] && TARGETS[$k]="$v"
done <<< "$SCRIBA_TARGETS"

"$BUILD_DIR/scriba" /tmp/kitchensink.md &
PID=$!
sleep 3
WID=$(xdotool search --onlyvisible --name "Scriba" | head -1)
xdotool windowsize "$WID" 1280 800
xdotool windowfocus "$WID"
sleep 3

# waitwin <title> -> echoes window id once it appears (or empty)
waitwin() {
    for _ in $(seq 1 60); do
        W=$(xdotool search --onlyvisible --name "$1" | head -1)
        if [ -n "$W" ]; then echo "$W"; return 0; fi
        sleep 0.2
    done
    return 1
}

# capture <title> <outfile> [sleep] [close-popup]: wait for dialog, let
# WebEngine render, shoot.  When close-popup is set, send one Escape before
# the capture to dismiss any open QComboBox dropdown (override-redirect X11
# window that import -window <dialog> can't composite, leaving black).
capture() {
    local W
    W=$(waitwin "$1") || { echo "WARN: window \"$1\" not found"; return 1; }
    sleep "${3:-4}"
    if [ "${4:-}" = "close-popup" ]; then
        xdotool key Escape; sleep 0.5
    fi
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

# ============================================================================
# One shot_<name>() function + one TARGETS entry per screenshot. To add a new
# capture:
#   1. add a shot_<name>() function here that opens its dialog/page and
#      `import -window` the shot into $OUT_DIR/<name>.png,
#   2. add one "<name>=\"<fn>|<one-line help>\" line to the TARGETS table at
#      the top of this file -- run_all(), the dispatch below, argument
#      validation and --help all derive from that table automatically,
#   3. for a new Preferences page, also add its name to PREF_PAGES (top of
#      file) and the idx mapping in shot_preference_page.
# This keeps every capture scriptable on its own, so a change to one dialog
# only regenerates that image instead of the whole slow suite.
# ============================================================================

shot_main() {
    import -window "$WID" "$OUT_DIR/screenshot.png"
    echo "  -> $OUT_DIR/screenshot.png"
}

# Three-tab tab bar (divider line only shows when the tab bar is visible,
# i.e. with 2+ tabs), so open two extra documents and select the second one
# for the shot. The second tab file is named "tab bar.md" so its tab label
# reads "tab bar" in the capture.
shot_tabbar() {
    printf "# Notes\n\nSecond document for the tab bar capture\n" > "/tmp/tab bar.md"
    printf "# Draft\n\nThird document for the tab bar capture\n" > /tmp/scriba-tab3.md
    open ctrl+o
    sleep 2
    xdotool type -- "/tmp/tab bar.md"
    sleep 1
    xdotool key Return
    sleep 5
    open ctrl+o
    sleep 2
    xdotool type -- "/tmp/scriba-tab3.md"
    sleep 1
    xdotool key Return
    sleep 5
    open ctrl+shift+tab
    sleep 2
    import -window "$WID" "$OUT_DIR/tabbar.png"
    # The three-tab shot is more readable cropped to the top: tab bar plus the
    # preview content just below it, skipping the empty editor/preview expanse
    # and the status bar.
    convert "$OUT_DIR/tabbar.png" -crop 1280x185+0+0 +repage "$OUT_DIR/tabbar.png"
    echo "  -> $OUT_DIR/tabbar.png"
    open ctrl+w
    sleep 1
    open ctrl+w
    sleep 1
}

# Small gutter crop: the chart-edit pencil in the gutter beside the mermaid
# fence lines. Open a dedicated doc whose mermaid block is the first thing, so
# the pencil (drawn on the first content line, the row right after the opening
# fence) sits at the top of the active document. The crop band covers just the
# gutter strip and the first couple of source lines — nothing of the tab bar,
# preview pane or the rest of the document.
shot_gutter_pencil() {
    printf '```mermaid\nflowchart LR\n  A[Write] --> B{Preview?}\n  B --> |Yes| C[Live render]\n  B --> |No| D[Keep typing]\n  C --> D\n```\n' > /tmp/scriba-gutter.md
    open ctrl+o
    sleep 2
    xdotool type -- "/tmp/scriba-gutter.md"
    sleep 1
    xdotool key Return
    sleep 5
    import -window "$WID" "$OUT_DIR/_gutter-full.png"
    # crop width = gutter strip (~45px) + ~295px of editor source (the fence +
    # one chart line); slim vertical band around the pencil row so nothing of
    # the tab bar (above) or preview panes leaks in. Y tuned to the fence +
    # pencil + one chart line.
    convert "$OUT_DIR/_gutter-full.png" -crop 340x68+0+74 +repage "$OUT_DIR/gutter-pencil.png"
    rm -f "$OUT_DIR/_gutter-full.png"
    echo "  -> $OUT_DIR/gutter-pencil.png"
    open ctrl+w
    sleep 1
}

# Capture one Preferences page. The sidebar list order matches PREF_PAGES; the
# (60,55) click selects the top row ("general"), and a page at list index `idx`
# needs `idx` Down presses after the click. (55 = below the settings search box.)
shot_preference_page() {
    local page="$1" idx
    case "$page" in
        general) idx=0 ;;
        themes) idx=1 ;;
        editor) idx=2 ;;
        preview) idx=3 ;;
        printing) idx=4 ;;
        advanced) idx=5 ;;
        writing) idx=6 ;;
        typography) idx=7 ;;
        replacements) idx=8 ;;
        spelling) idx=9 ;;
        corpus) idx=10 ;;
        security) idx=11 ;;
        *) echo "WARN: unknown preferences page \"$page\""; return 1 ;;
    esac
    open ctrl+alt+p
    PREF=$(waitwin "Preferences") || { echo "WARN: Preferences window not found"; return 1; }
    sleep 1
    xdotool mousemove --window "$PREF" 60 55 click 1
    sleep 1
    for ((i = 0; i < idx; i++)); do
        xdotool key Down
        sleep 0.5
    done
    sleep 1
    import -window "$PREF" "$OUT_DIR/preferences-$page.png"
    echo "  -> $OUT_DIR/preferences-$page.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

shot_preferences() {
    open ctrl+alt+p
    PREF=$(waitwin "Preferences") || { echo "WARN: Preferences window not found"; return 1; }
    sleep 1
    xdotool mousemove --window "$PREF" 60 55 click 1
    sleep 1
    import -window "$PREF" "$OUT_DIR/preferences-general.png"
    echo "  -> $OUT_DIR/preferences-general.png"
    for PAGE in "${PREF_PAGES[@]:1}"; do
        xdotool key Down
        sleep 1
        import -window "$PREF" "$OUT_DIR/preferences-$PAGE.png"
        echo "  -> $OUT_DIR/preferences-$PAGE.png"
    done
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

# --- Table helper ---
shot_table_dialog() { open ctrl+t; capture "Insert Table" "$OUT_DIR/table-dialog.png"; }

# --- Emoji picker ---
shot_emoji_picker() { open ctrl+e; capture "Emoji Picker" "$OUT_DIR/emoji-picker.png"; }

# --- KaTeX helper (type a sample equation so the preview renders) ---
shot_katex_dialog() {
    open ctrl+k
    KATEX=$(waitwin "Insert Equation") || { echo "WARN: Insert Equation window not found"; return 1; }
    sleep 1
    xdotool type -- "E = mc^2"
    sleep 4
    import -window "$KATEX" "$OUT_DIR/katex-dialog.png"
    echo "  -> $OUT_DIR/katex-dialog.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

# --- Mchem helper (type a sample formula so the preview renders) ---
shot_mchem_dialog() {
    open ctrl+shift+k
    MCHEM=$(waitwin "Insert Chemistry Notation") || { echo "WARN: Insert Chemistry Notation window not found"; return 1; }
    sleep 1
    xdotool type -- "2H_2 + O_2 -> 2H_2O"
    sleep 4
    import -window "$MCHEM" "$OUT_DIR/mchem-dialog.png"
    echo "  -> $OUT_DIR/mchem-dialog.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

# --- Chart builder (ECharts, renders sample chart on open) ---
shot_chart_dialog() { open ctrl+g; capture "Chart Builder" "$OUT_DIR/chart-dialog.png"; }

# --- Stock chart builder (candlestick with volume + MAs) ---
shot_stock_chart_dialog() { open ctrl+alt+s; capture "Stock Chart Builder" "$OUT_DIR/stock-chart-dialog.png"; }

# --- Advanced Charts builder (sankey default, live preview) ---
# The action has no shortcut; open the &Tools menu and pick "&Advanced
# Charts..." by its Alt-mnemonic (a).
shot_advanced_charts() {
    xdotool windowfocus "$WID"
    sleep 0.2
    xdotool key alt+t
    sleep 0.4
    xdotool key a
    sleep 0.4
    xdotool key Return
    capture "Advanced Charts" "$OUT_DIR/advanced-charts.png" 4 close-popup
}

# --- Mermaid chart helper (default pie chart) ---
shot_mermaid_dialog() { open ctrl+m; capture "Mermaid Diagrams" "$OUT_DIR/mermaid-dialog.png"; }

# --- Mermaid Git Graph helper: build a small fixture repo in /tmp (fast,
# deterministic shot) and load it into the panel. The repo-path field is
# focused by mouse-click, NOT Tab: Tab does not land in the field, and a
# Return with focus elsewhere activates the dialog's default Insert button,
# which closes the dialog and leaves nothing to capture.
shot_mermaid_gitgraph() {
    local demo=/tmp/scriba-gitgraph-demo
    rm -rf "$demo"
    mkdir -p "$demo"
    (
        cd "$demo" || exit 1
        git init -q -b main
        git config user.name "Demo"
        git config user.email "demo@example.com"
        for n in 1 2 3; do
            printf 'commit %s\n' "$n" >> README.md
            git add README.md
            GIT_AUTHOR_DATE="2026-01-0${n}T10:00:00Z" GIT_COMMITTER_DATE="2026-01-0${n}T10:00:00Z" git commit -qm "c$n"
        done
        git checkout -qb feature
        for n in 4 5; do
            printf 'feature %s\n' "$n" >> feature.txt
            git add feature.txt
            GIT_AUTHOR_DATE="2026-01-0${n}T10:00:00Z" GIT_COMMITTER_DATE="2026-01-0${n}T10:00:00Z" git commit -qm "c$n"
        done
        git checkout -q main
        GIT_AUTHOR_DATE="2026-01-06T10:00:00Z" GIT_COMMITTER_DATE="2026-01-06T10:00:00Z" git merge feature --no-ff --no-edit -q
    ) || { echo "WARN: could not create fixture repo"; return 1; }

    open ctrl+m
    MDG=$(waitwin "Mermaid Diagrams") || { echo "WARN: Mermaid Diagrams window not found"; return 1; }
    sleep 1
    # Jump the chart-type combo to "Git Graph" (index 12): Home resets to Pie,
    # then 12 Downs.
    xdotool key Home
    sleep 0.3
    for ((i = 0; i < 12; i++)); do xdotool key Down; sleep 0.05; done
    sleep 0.5
    # Open the Browse file dialog (Alt+B), navigate to the fixture repo via
    # the GTK location bar (Ctrl+L), then confirm with two Returns.
    xdotool key alt+b
    sleep 2
    local FD
    FD=$(xdotool search --onlyvisible --name "Select Git Repository" | head -1)
    if [ -z "$FD" ]; then
        echo "WARN: Browse dialog not found"; xdotool key Escape; return 1
    fi
    xdotool windowfocus "$FD"; sleep 1
    xdotool key ctrl+l; sleep 0.5
    xdotool type -- "$demo"; sleep 0.5
    xdotool key Return; sleep 1   # navigate to directory
    xdotool key Return; sleep 1   # confirm selection (Open)
    # Give libgit2 + WebEngine time to walk the repo and render the graph.
    sleep 8
    if ! xdotool search --onlyvisible --name "Mermaid Diagrams" >/dev/null 2>&1; then
        echo "WARN: Mermaid Diagrams closed before capture"; return 1
    fi
    # Close any open QComboBox dropdown overlay (same black-box artifact as
    # advanced-charts; see capture() comment).
    xdotool key Escape; sleep 0.5
    import -window "$MDG" "$OUT_DIR/mermaid-gitgraph.png"
    echo "  -> $OUT_DIR/mermaid-gitgraph.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

# --- Check Spelling (type a misspelled word so the dialog has an error) ---
# NB: `xdotool key F7` latches a phantom Alt before the key, so Qt sees
# Alt+F7. Send the raw keycode (73 = F7 on the standard X keymap) instead.
shot_check_spelling() {
    xdotool key ctrl+End
    xdotool type -- "helo "
    sleep 1
    xdotool windowfocus "$WID"
    sleep 0.2
    xdotool keydown 73
    xdotool keyup 73
    capture "Check Spelling" "$OUT_DIR/check-spelling.png"
}

# --- Validation Report options (choose which checks to run) ---
# NB: like the F7 above, `xdotool key` with F7 latches a phantom Alt, so send
# the raw keycode (73 = F7) with Ctrl+Shift held down instead.
shot_validation_report() {
    xdotool windowfocus "$WID"
    sleep 0.2
    xdotool keydown ctrl keydown shift keydown 73 keyup 73 keyup shift keyup ctrl
    capture "Validation Report Options" "$OUT_DIR/validation-report.png"
}

# --- Print / Export PDF (long wait: WebEngine load, mermaid/echarts promises,
# chromium --print-to-pdf, then preview reload) ---
shot_print_pdf_dialog() {
    open ctrl+p
    capture "Print / Export PDF" "$OUT_DIR/print-pdf-dialog.png" 20
}

# --- Table of Contents (Corpus → View Table of Contents / Ctrl+Shift+T) ---
# Build a small fixture corpus (in-root docs + one out-of-root doc), open it
# via File → Corpus → Open Corpus… (alt+f, Down to the Corpus submenu, Right,
# then the 'o' mnemonic), then Ctrl+Shift+T. The capture shows the read-only
# TOC tab active, with the rendered TOC in the preview pane.
shot_toc() {
    local demo=/tmp/scriba-toc-demo
    rm -rf "$demo"
    mkdir -p "$demo/docs"
    printf '# Alpha\n\nIntro to Alpha.\n\n## Sub A\n\nDetail under Alpha.\n\n### Deep A\n\nDeepest.\n' > "$demo/docs/a.md"
    printf '# Beta\n\nIntro to Beta.\n' > "$demo/docs/b.md"
    printf '## External\n\nOut-of-root document.\n' > "$demo/ext.md"
    cat > "$demo/corpus.scriba" <<JSON
{
    "version": 1,
    "name": "TOC Demo",
    "active": 0,
    "monitor": false,
    "dictionary": {},
    "documents": [
        { "path": "docs/a.md" },
        { "path": "docs/b.md" },
        { "path": "$demo/ext.md" }
    ]
}
JSON

    xdotool windowfocus "$WID"
    sleep 0.2
    xdotool key alt+f
    sleep 0.5
    xdotool key Down          # highlight "C&orpus" submenu
    sleep 0.3
    xdotool key Right         # open the Corpus submenu
    sleep 0.5
    xdotool key o             # highlight &Open Corpus… ('o' also matches File → &Open…, so Return confirms)
    sleep 0.3
    xdotool key Return
    sleep 2
    local FD
    FD=$(xdotool search --onlyvisible --name "Open Corpus" | head -1)
    if [ -z "$FD" ]; then
        echo "WARN: Open Corpus dialog not found"; xdotool key Escape; return 1
    fi
    xdotool windowfocus "$FD"; sleep 1
    xdotool key ctrl+l; sleep 0.5
    xdotool type -- "$demo/corpus.scriba"; sleep 0.5
    xdotool key Return; sleep 1   # jump to the directory
    xdotool key Return; sleep 3   # confirm Open, let the docs load
    open ctrl+shift+t
    sleep 5                       # WebEngine renders the TOC preview
    import -window "$WID" "$OUT_DIR/toc.png"
    echo "  -> $OUT_DIR/toc.png"
    xdotool key Escape
    sleep 1
    xdotool windowfocus "$WID"
}

run_all() {
    for t in "${TARGET_ORDER[@]}"; do
        "${TARGETS[$t]%%|*}"
    done
}

# Resolve one target name to its shot function and run it. Target names come
# from the TARGETS table; preferences-<page> and bare PREF_PAGES page names
# both resolve to shot_preference_page. (Unknown names here are unreachable
# because the outer script validates args first, but kept for safety.)
run_target() {
    local t="$1"
    case "$t" in
        preferences-*) shot_preference_page "${t#preferences-}"; return ;;
    esac
    if [[ -n "${TARGETS[$t]+x}" ]]; then
        "${TARGETS[$t]%%|*}"
        return
    fi
    for p in "${PREF_PAGES[@]}"; do
        if [[ "$p" = "$t" ]]; then
            shot_preference_page "$t"
            return
        fi
    done
    echo "WARN: unknown target \"$t\" (run scripts/update-screenshot.sh --help for the list)"
}

# --- dispatch ---
if [ "$1" = "all" ]; then
    run_all
else
    for t in "$@"; do
        run_target "$t"
    done
fi

kill $PID 2>/dev/null
wait $PID 2>/dev/null
_SCRIBASH_

echo "Updated screenshots in $OUT_DIR"