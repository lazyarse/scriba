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
QString handleTableReturn(const QString &line, const QString &prevLine);
QString makeEmptyTableRow(int cols);
QString makeEmptyHtmlTableRow(int cols);
int tableNavCell(const QString &line, int cursorPos, bool forward);
int tableNavHtmlCell(const QString &line, int cursorPos, bool forward);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);
QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column);

// Path/emoji completion (link, HTML attribute, and emoji shortcode contexts)
bool extractLinkPath(const QString &line, int cursorPos, QString &partialPath);
int linkPathReplaceStart(const QString &line, int cursorPos);
bool extractHtmlPath(const QString &line, int cursorPos, QString &value);
int htmlPathReplaceStart(const QString &line, int cursorPos);
bool extractEmojiCode(const QString &line, int cursorPos, QString &partialCode);

struct FileCompletionResult
{
    QStringList entries;
    QString filePart;
};
FileCompletionResult matchFileEntries(const QString &partialPath, const QDir &baseDir,
                                      int limit = 20);

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

namespace Debounce {
    constexpr int PreviewUpdate = 80;   // MainWindow live-preview push timer
    constexpr int AnchorScroll  = 300;  // MainWindow scroll-sync anchor timer
    constexpr int FoldScan      = 300;  // Editor fold rescan after editing
    constexpr int SpellCheck    = 400;  // SpellHighlighter spell/grammar timers
    constexpr int DialogPreview = 300;  // Mermaid/KaTeX/Mchem/Vega dialog previews
    constexpr int HeavyRender   = 1500; // JS heavy-render setTimeout in preview
    constexpr int TabSwitchRender = 100; // JS heavy-render delay after a tab switch
}

class QWebEngineView;
