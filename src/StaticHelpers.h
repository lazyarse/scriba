// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

#include <QString>
#include <QStringList>
#include <QDir>
#include <QTextCursor>
#include <QIcon>
#include <QChar>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QTimer>

inline const QChar clearSentinel(0x2412);

QString escapeJsString(const QString &s);
bool isThematicBreak(const QString &line);
QString handleListReturn(const QString &line);
// Returns the list-marker prefix to place on the new line when Enter is pressed
// mid-way through a list item (caret inside the item's content with more text
// after it), or an empty QString when the caret isn't in a list item's content
// (inside the marker itself, at the very end of the line, at the start, in
// plain text, or on a thematic break) — Enter then falls back to a plain split.
QString handleListSplitReturn(const QString &line, int caretPos);
QString handleTableReturn(const QString &line, const QString &prevLine);
// Returns whether `line` is a markdown table separator row: pipes where every
// cell is made only of optional colons and at least one dash (spaces allowed
// around it). Matches narrow columns such as `|:--:|` or `|--:|` too, not just
// the spec's three-dash minimum.
bool isMdSeparatorRow(const QString &line);
// Returns whether `line` is a markdown table data row whose cells are all
// empty (e.g. `|   |   |`). Separator rows are never blank.
bool isBlankMdTableRow(const QString &line);
QString makeEmptyTableRow(int cols);
QString makeEmptyHtmlTableRow(int cols);
int tableNavCell(const QString &line, int cursorPos, bool forward);
int tableNavHtmlCell(const QString &line, int cursorPos, bool forward);
// Re-aligns a markdown table: takes the contiguous `|`-delimited rows of a
// table block (header, separator, and data rows) and returns the rows re-spaced
// so the column pipes line up. Per-column alignment follows the separator row
// (`:---` left, `---:` right, `:---:` center, `---` default/left). Returns an
// empty QString when `rows` is not a valid table (no separator row containing
// `---`). Cell content is trimmed; escaped pipes (`\|`) are preserved.
QString formatMdTable(const QStringList &rows);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);
QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column);

// Returns whether `filePath` is a local image file safe to embed in the
// preview: it must exist, be a regular file, and have an image extension.
bool isSafePreviewImage(const QString &filePath);

// Path/emoji completion (link, HTML attribute, and emoji shortcode contexts)
bool extractLinkPath(const QString &line, int cursorPos, QString &partialPath);
int linkPathReplaceStart(const QString &line, int cursorPos);
bool extractHtmlPath(const QString &line, int cursorPos, QString &value);
int htmlPathReplaceStart(const QString &line, int cursorPos);
bool extractEmojiCode(const QString &line, int cursorPos, QString &partialCode);

// Shared emoji catalog and rendering (used by both the editor autocomplete and
// the Emoji Picker dialog). Parsed once from :/emoji.js; `codePoint` is the
// twemoji SVG filename stem (unqualified, fe0f stripped when possible).
struct EmojiEntry {
    QString shortcode;
    QString unicode;
    QString codePoint;
};
QList<EmojiEntry> emojiCatalog();
// Resolves a unicode emoji string to its bundled twemoji resource path
// (:/twemoji/svg/<stem>.svg) or an empty QString when no SVG is available.
QString emojiTwemojiPath(const QString &unicode);
// Renders `unicode` into a `size`x`size` pixmap honoring the EmojiMode setting:
// color renders the twemoji SVG, bw the Symbola glyph. Transparent background.
QPixmap renderEmojiPixmap(const QString &unicode, int size);

// Typo autocorrect: returns the replacement for the word ending at cursorPos, or
// an empty QString when no configured "typo=replacement" pair applies. Matching
// is case-insensitive and the replacement preserves the typed word's case
// ("teh"->"the", "Teh"->"The", "TEH"->"THE"). When `separatorTyped` is true the
// word must be followed by at least one non-letter (the separator just typed),
// so mid-word keystrokes never correct. The word may contain apostrophes and
// hyphens ("should'nt"). The character before the word must be a word boundary,
// so URLs/emails/paths/identifiers are left alone. On a match, *wordStart and
// *wordLength receive the word's extent within `line` (word only, excluding any
// trailing separator).
QString autoCorrectWord(const QString &line, int cursorPos, const QStringList &pairs,
                        bool separatorTyped, int *wordStart = nullptr, int *wordLength = nullptr);

struct FileCompletionResult
{
    QStringList entries;
    QString filePart;
};
FileCompletionResult matchFileEntries(const QString &partialPath, const QDir &baseDir,
                                      int limit = 20);

// Fuzzy (sequential) subsequence match. Every character of `fragment` appears
// in `entry` in order, possibly with skipped characters in between. Subsumes
// plain substring containment, so "scrsvg" matches "scriba.svg" and "prt"
// matches "pirate". An empty fragment matches everything. Lower `gaps` (chars
// skipped between matched chars) and lower `firstPos` rank a match as tighter,
// so a prefix match always scores {true, 0, 0} and ranks first.
struct FuzzyScore {
    bool matched = false;
    int gaps = 0;
    int firstPos = 0;
};
FuzzyScore fuzzyMatchScore(const QString &entry, const QString &fragment);

QIcon themedIcon(const QString &svgPath, const QColor &color, int size = 28);

void stripButtonIcon(QAbstractButton *btn);
void stripButtonIcons(QDialogButtonBox *box);

QString readResourceFile(const QString &path);
QString duplicateCssFile(const QString &sourcePath, const QString &destDir, const QString &baseName = QString());

class DebounceTimer : public QTimer
{
    Q_OBJECT
public:
    explicit DebounceTimer(int interval, QObject *parent = nullptr);
    void arm();
};

// C++ QTimer debounce timers: cheap "light" edits debounce on LightRender,
// then the expensive render pass follows after HeavyRender (a tab switch
// shortens that wait via TabSwitchRender).
namespace Debounce {
    constexpr int LightRender      = 80;   // live-preview push debounce
    constexpr int AnchorScroll     = 300;  // scroll-sync anchor timer
    constexpr int FoldScan         = 300;  // Editor fold rescan after editing
    constexpr int SpellCheck       = 400;  // SpellHighlighter spell/grammar timers
    constexpr int DialogPreview    = 300;  // Mermaid/KaTeX/Mchem/ECharts dialog previews
    constexpr int HeavyRender      = 1500; // JS heavy-render setTimeout in preview
    constexpr int TabSwitchRender  = 100;  // JS heavy-render delay after a tab switch
}

// C++ non-debounce timing values that are not embedded in JS strings.
namespace Timeout {
    constexpr int RenderTimeoutMs = 30000; // hard cap for renderSync/HTML import
    constexpr int ReportProgress  = 150;   // validation-report progress poll
}

// Timing values that live inside generated JS strings (injected at build
// time from these constants; keep the JS snippets in sync).
namespace JsTiming {
    constexpr int AnchorNavRetry      = 300;  // anchor-jump retry poll
    constexpr int ChartLayoutPoll     = 50;   // ECharts container-size poll
    constexpr int ChartLayoutTries    = 40;   // ECharts size-poll retry cap
    constexpr int EChartsReadyTimeout = 2000; // ECharts-ready fallback resolve
}

class QWebEngineView;
