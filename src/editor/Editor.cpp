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
#include "Editor.h"
#include "corpus/Corpus.h"
#include "spell/GrammarChecker.h"
#include "Gutter.h"
#include "EditorScrollBar.h"
#include "IssueSummaryPane.h"
#include "validation/MdLintConfig.h"
#include "prefs/Preferences.h"
#include "spell/SpellChecker.h"
#include "spell/StoppardEngine.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QToolTip>
#include <QAbstractTextDocumentLayout>
#include <QFileInfo>

namespace {


// StoppardEngine is stateless and cheap to construct (no dictionary load), so
// each Editor tab gets its own instance.
GrammarChecker *sharedGrammarChecker()
{
    QSettings settings;
    const QString dialect = settings
        .value(Preferences::GrammarDialect, QStringLiteral("American"))
        .toString();
    return new StoppardEngine(dialect);
}

// Reads the markdown-lint rule configuration from `settings`. When the master
// `MarkdownCheckEnabled` is off no rules run (empty config).
MdLintConfig markdownConfigFromSettings(const QSettings &settings)
{
    if (!settings.value(Preferences::MarkdownCheckEnabled, false).toBool())
        return {};
    return MdLintConfig::fromJson(settings.value(Preferences::MarkdownLintConfig).toString());
}


} // namespace

Editor::Editor(QWidget *parent)
    : QTextEdit(parent)
{
    setObjectName("scriba-editor");
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    setFrameShape(QFrame::NoFrame);

    loadEmojiShortcodes();

    setupGutter();

    m_errorScrollBar = new EditorScrollBar(this);
    setVerticalScrollBar(m_errorScrollBar);

    auto *foldTimer = new QTimer(this);
    foldTimer->setSingleShot(true);
    foldTimer->setInterval(Debounce::FoldScan);
    connect(foldTimer, &QTimer::timeout, this, &Editor::scanHeadersAndFolds);
    connect(document(), &QTextDocument::contentsChanged, this, [this, foldTimer]() {
        if (!m_updatingFolds)
            foldTimer->start();
    });

    m_spellChecker = std::make_unique<SpellChecker>();
    m_grammarChecker.reset(sharedGrammarChecker());
    m_spellHighlighter = new SpellHighlighter(document(), this);
    m_spellHighlighter->setChecker(m_spellChecker.get());
    m_spellHighlighter->setGrammarChecker(m_grammarChecker);
    m_errorScrollBar->setHighlighter(m_spellHighlighter);
    applySpellSettings();
    applyLineWrap();

    m_underlineOverlay = new QWidget(viewport());
    m_underlineOverlay->setObjectName(QStringLiteral("underline-overlay"));
    m_underlineOverlay->setAttribute(Qt::WA_TranslucentBackground);
    m_underlineOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
    m_underlineOverlay->installEventFilter(this);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    connect(document(), &QTextDocument::contentsChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    // Spell underlines are painted from the spell-hit cache, which is
    // refreshed asynchronously (word-boundary or debounced check): repaint
    // the overlay when a check completes.
    connect(m_spellHighlighter, &SpellHighlighter::spellHitsChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    connect(m_spellHighlighter, &SpellHighlighter::spellHitsChanged,
            m_errorScrollBar, &EditorScrollBar::invalidate);
    connect(document(), &QTextDocument::contentsChanged,
            m_errorScrollBar, &EditorScrollBar::invalidate);
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            m_errorScrollBar, &EditorScrollBar::invalidate);

    // Issue-summary pane: live counts over the highlighter's caches. It is
    // child of the viewport like the underline overlay, but stays top-right
    // and only the [x] button is interactive.
    m_issueSummaryPane = new IssueSummaryPane(viewport());
    connect(m_spellHighlighter, &SpellHighlighter::spellHitsChanged,
            this, &Editor::updateIssueSummary);
    connect(m_issueSummaryPane, &IssueSummaryPane::closeRequested, this, [this]() {
        m_issueSummaryDismissed = true; // don't re-show on every keystroke
    });
    m_issueSummaryShowTimer = new QTimer(this);
    m_issueSummaryShowTimer->setSingleShot(true);
    m_issueSummaryShowTimer->setInterval(Debounce::IssueSummary);
    connect(m_issueSummaryShowTimer, &QTimer::timeout,
            this, &Editor::onIssueSummaryShow);

    connect(this, &Editor::cursorPositionChanged,
            this, &Editor::onCursorPositionChanged);
    // Any text change while the cursor is inside a table marks it dirty so it
    // gets re-aligned when the cursor leaves. The formatting guard stops the
    // reformat from re-marking itself.
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        if (m_trackTableStartBlock >= 0 && !m_formattingTable)
            m_tableDirty = true;
    });
}

Editor::~Editor() = default;

void Editor::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    // Relative link targets resolve against this directory — re-check so the
    // broken-link underlines follow the new base.
    if (m_spellHighlighter)
        m_spellHighlighter->setCurrentFile(path);
    if (m_issueSummaryOptions.enabled)
        updateIssueSummary();
}

void Editor::setFallbackLinkBaseDir(const QString &dir)
{
    if (m_spellHighlighter)
        m_spellHighlighter->setFallbackLinkBaseDir(dir);
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    // Escape: dismiss the completer popup.
    if (event->key() == Qt::Key_Escape
            && m_completer && m_completer->popup()->isVisible()
            && handleEscapeKey())
        return;

    // Return/Enter: completer accept, list continuation, table rows, fold return.
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && handleEnterKey(event))
        return;

    // Tab/Backtab: completion cycling, block indent/dedent, list and table
    // navigation. Falls through (returns false) for an inert Backtab so the
    // default handling below runs.
    if ((event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)
            && handleTabKey(event))
        return;

    // Down: fold expand, but only when no completion popup is showing — the
    // popup owns Down while it is open.
    if (event->key() == Qt::Key_Down
            && (!m_completer || !m_completer->popup()->isVisible())
            && handleDownKey())
        return;

    bool ctrl = event->modifiers() & Qt::ControlModifier;
    bool alt = event->modifiers() & Qt::AltModifier;

    // Ctrl+Alt+Up/Down: scroll viewport
    if (ctrl && alt && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            && handleCtrlAltScroll(event->key())) {
        event->accept();
        return;
    }

    // Ctrl+Up/Down: jump to prev/next header
    if (ctrl && !alt && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
            && handleHeaderJump(event->key())) {
        event->accept();
        return;
    }

    // Ctrl+= expand, Ctrl+- fold
    if (ctrl && !alt && (event->key() == Qt::Key_Equal || event->key() == Qt::Key_Minus)
            && handleZoom(event->key())) {
        event->accept();
        return;
    }

    // Ctrl+D: hardwrap (copy the current block below, or the selection).
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_D
            && handleHardwrap()) {
        event->accept();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Y) {
        deleteLine();
        event->accept();
        return;
    }

    // Backspace/Delete: perform the default deletion, then keep the completer
    // popup in sync with the edited text.
    if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)
            && handleBackspaceDelete(event))
        return;

    QTextEdit::keyPressEvent(event);

    if (!event->text().isEmpty()) {
        // A space or punctuation just completed the word before the cursor, so
        // apply any configured typo replacement before re-evaluating completions.
        applyAutoCorrect(true);
        QChar c = event->text()[0];
        bool shown = false;
        if (c.isLetterOrNumber() || c == '_' || c == ':' || c == '+' || c == '-' || c == '.' || c == '/') {
            QString partialPath;
            if (isInsideLinkContext(textCursor(), partialPath))
                shown = showFileCompletion(partialPath);
            else {
                QString htmlPath;
                if (isInsideHtmlPathContext(textCursor(), htmlPath))
                    shown = showFileCompletion(htmlPath);
            }
            QString partialCode;
            if (isInsideEmojiContext(textCursor(), partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool())
                shown = showEmojiCompletion(partialCode) || shown;
            QString partialLang;
            if (isInsideLanguageContext(textCursor(), partialLang) && QSettings().value(Preferences::LanguageAutoComplete, true).toBool())
                shown = showLanguageCompletion(partialLang) || shown;
        }
        if (!shown && m_completer && m_completer->popup()->isVisible())
            m_completer->popup()->hide();
    }
}

void Editor::insertFromMimeData(const QMimeData *source)
{
    const int posBefore = textCursor().position();
    QTextEdit::insertFromMimeData(source);
    // A paste that drops a whole markdown table bypasses the leave-to-format
    // tracking in onCursorPositionChanged: the insert's contentsChanged fires
    // before the cursor lands inside the table, so the table is never marked
    // dirty and clicking away would not realign it. Align right away, like the
    // Insert Table dialog does. No-op for non-table content.
    formatTableAt(posBefore);
}

bool Editor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_underlineOverlay && event->type() == QEvent::Paint && m_spellHighlighter) {
        QPaintEvent *pe = static_cast<QPaintEvent*>(event);
        QPainter painter(m_underlineOverlay);
        painter.setClipRect(pe->rect());
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QFontMetrics fm(font());
        const int underlineY = fm.ascent() + fm.underlinePos() + kUnderlineDropPx;
        const QTextDocument *doc = document();
        for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
            if (!block.isVisible())
                continue;
            const int blockNumber = block.blockNumber();
            const QTextLayout *layout = block.layout();
            if (!layout || layout->lineCount() == 0)
                continue;
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->spellHitsInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::spellUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->grammarIssuesInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::grammarUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->linkIssuesInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::linkUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->markdownHitsInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::markdownUnderlineColor(), fm, underlineY);
        }
        return false;
    }
    return QTextEdit::eventFilter(obj, event);
}

void Editor::paintHitRange(QPainter &painter, const QTextBlock &block, int start, int length,
                           const QColor &color, const QFontMetrics &fm, int underlineY)
{
    const QTextLayout *layout = block.layout();
    if (!layout || layout->lineCount() == 0)
        return;
    painter.setPen(QPen(color, kUnderlinePenWidthPx, Qt::SolidLine, Qt::FlatCap));
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        const int a = qMax(start, line.textStart());
        const int b = qMin(start + length, line.textStart() + line.textLength());
        if (a >= b)
            continue;
        QTextCursor cursor(document());
        cursor.setPosition(block.position() + a);
        const QRect leftRect = cursorRect(cursor);
        cursor.setPosition(block.position() + b);
        const QRect rightRect = cursorRect(cursor);
        const int y = leftRect.top() + underlineY;
        painter.drawLine(QPoint(leftRect.left(), y), QPoint(rightRect.left(), y));
    }
}

void Editor::applySpellSettings()
{
    if (!m_spellChecker || !m_spellHighlighter)
        return;
    SpellHighlighter::reloadUnderlineColors();
    QSettings s;
    const bool spellEnabled = s.value(Preferences::SpellCheckEnabled, true).toBool();
    const bool grammarEnabled = s.value(Preferences::GrammarCheckEnabled, false).toBool();
    const bool linkEnabled = s.value(Preferences::LinkCheckEnabled, true).toBool();
    const bool markdownEnabled = s.value(Preferences::MarkdownCheckEnabled, false).toBool();

    // Empty = "follow dialect": the grammar dialect selects the base dictionary.
    const QString language = s.value(Preferences::DictionaryLanguage).toString();
    const QString dialect = s.value(Preferences::GrammarDialect, QStringLiteral("American")).toString();

    m_spellChecker->setDialect(dialect);

    bool loaded = false;
    if (spellEnabled) {
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect)
            : language;
        loaded = m_spellChecker->loadLanguage(resolved);
        if (!loaded) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (m_spellChecker->loadLanguage(lang)) {
                    loaded = true;
                    break;
                }
            }
        }
    }
    m_spellHighlighter->setSpellCheckingEnabled(spellEnabled && loaded);
    m_spellHighlighter->setGrammarCheckingEnabled(grammarEnabled);
    m_spellHighlighter->setLinkCheckingEnabled(linkEnabled);
    m_spellHighlighter->setMarkdownCheckingEnabled(markdownEnabled);
    m_spellHighlighter->setMarkdownConfig(markdownConfigFromSettings(s));

    if (auto *stoppard = dynamic_cast<StoppardEngine *>(m_grammarChecker.get()))
        stoppard->setDialect(dialect);

    m_spellHighlighter->refresh();
    if (m_errorScrollBar)
        m_errorScrollBar->applySettings();
}

void Editor::recheckSpelling()
{
    applySpellSettings();
}

void Editor::applyCorpusDictionary(const CorpusDictionary &dict, bool merge)
{
    if (!m_spellChecker || !m_spellHighlighter)
        return;
    m_corpusActive = true;
    m_spellChecker->setCorpusMerge(merge);
    m_spellChecker->setCorpusWords(dict.customWords);
    m_spellChecker->setCorpusIgnored(dict.ignoredWords);

    // The corpus language/dialect override the global preferences when set;
    // empty fields fall back to the per-user settings, mirroring
    // applySpellSettings().
    const QSettings s;
    const QString language = dict.language.isEmpty()
        ? s.value(Preferences::DictionaryLanguage).toString()
        : dict.language;
    const QString dialect = dict.dialect.isEmpty()
        ? s.value(Preferences::GrammarDialect, QStringLiteral("American")).toString()
        : dict.dialect;
    m_spellChecker->setDialect(dialect);

    const bool spellEnabled = s.value(Preferences::SpellCheckEnabled, true).toBool();
    bool loaded = false;
    if (spellEnabled) {
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect)
            : language;
        loaded = m_spellChecker->loadLanguage(resolved);
        if (!loaded) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (m_spellChecker->loadLanguage(lang)) {
                    loaded = true;
                    break;
                }
            }
        }
    }
    m_spellHighlighter->setSpellCheckingEnabled(spellEnabled && loaded);
    m_spellHighlighter->refresh();
}

void Editor::refreshUnderlines()
{
    SpellHighlighter::reloadUnderlineColors();
    if (m_errorScrollBar)
        m_errorScrollBar->applySettings();
    if (m_underlineOverlay)
        m_underlineOverlay->update();
}

void Editor::setIssueSummaryOptions(const IssueSummaryOptions &options,
                                    const QColor &themeBg, const QColor &themeFg)
{
    m_issueSummaryOptions = options;
    m_issueSummaryThemeBg = themeBg;
    m_issueSummaryThemeFg = themeFg;
    if (!m_issueSummaryPane)
        return;
    m_issueSummaryPane->setTheme(themeBg, themeFg);
    if (!options.enabled) {
        m_issueSummaryPane->hide();
        if (m_issueSummaryShowTimer)
            m_issueSummaryShowTimer->stop();
        return;
    }
    m_issueSummaryDismissed = false;
    updateIssueSummary();
}

void Editor::showIssueSummary()
{
    if (!m_issueSummaryOptions.enabled)
        return;
    m_issueSummaryDismissed = false;
    updateIssueSummary();
}

void Editor::updateIssueSummary()
{
    if (!m_issueSummaryPane || !m_issueSummaryOptions.enabled || !isMarkdownFile()) {
        if (m_issueSummaryPane)
            m_issueSummaryPane->hide();
        if (m_issueSummaryShowTimer)
            m_issueSummaryShowTimer->stop();
        return;
    }
    if (!m_spellHighlighter)
        return;

    const auto counts = m_spellHighlighter->counts();
    const QSet<IssueSummaryPane::Kind> &sel = m_issueSummaryOptions.categories;
    QVector<IssueSummaryPane::Row> rows;
    auto addRow = [&](IssueSummaryPane::Kind kind, const QString &label, int count, const QColor &color, bool engineOn) {
        if (sel.contains(kind) && engineOn)
            rows.append({kind, label, count, color});
    };
    addRow(IssueSummaryPane::Kind::Typos,
           QStringLiteral("Typos"), counts.spelling,
           SpellHighlighter::spellUnderlineColor(), m_spellHighlighter->spellCheckingEnabled());
    addRow(IssueSummaryPane::Kind::Grammar,
           QStringLiteral("Grammar"), counts.grammar,
           SpellHighlighter::grammarUnderlineColor(), m_spellHighlighter->grammarCheckingEnabled());
    addRow(IssueSummaryPane::Kind::Lint,
           QStringLiteral("Markdown"), counts.markdown,
           SpellHighlighter::markdownUnderlineColor(), m_spellHighlighter->markdownCheckingEnabled());
    if (counts.markdownWarnings > 0)
        addRow(IssueSummaryPane::Kind::Lint,
               QStringLiteral("Markdown warnings"), counts.markdownWarnings,
               SpellHighlighter::markdownUnderlineColor(), m_spellHighlighter->markdownCheckingEnabled());
    addRow(IssueSummaryPane::Kind::Links,
           QStringLiteral("Broken links"), counts.links,
           SpellHighlighter::linkUnderlineColor(), m_spellHighlighter->linkCheckingEnabled());

    if (rows.isEmpty()) {
        m_issueSummaryPane->hide();
        m_issueSummaryShowTimer->stop();
        return;
    }

    // Keep the row content current as the checkers land, but defer the show:
    // the first pass may run before any check finished (all-zero counts), and
    // showing inside the startup burst loses the initial paint. The debounce
    // timer shows the pane once, in a settled event loop, with final counts.
    m_issueSummaryPane->setRows(rows);
    m_issueSummaryShowTimer->stop();
    m_issueSummaryShowTimer->start();
}

void Editor::onIssueSummaryShow()
{
    if (!m_issueSummaryPane || m_issueSummaryDismissed)
        return;
    positionIssueSummaryPane();
    const int timeoutMs = m_issueSummaryOptions.timeoutEnabled
        ? m_issueSummaryOptions.timeoutSeconds * 1000 : 0;
    m_issueSummaryPane->showWithTimeout(timeoutMs);
    m_issueSummaryPane->update();
}

void Editor::positionIssueSummaryPane()
{
    if (!m_issueSummaryPane)
        return;
    const int margin = 12;
    const QSize sh = m_issueSummaryPane->sizeHint();
    if (sh.isEmpty())
        return;
    m_issueSummaryPane->setGeometry(viewport()->width() - sh.width() - margin,
                                    margin, sh.width(), sh.height());
}

bool Editor::isMarkdownFile() const
{
    // Untitled tabs (empty path) are markdown by construction: new docs and
    // corpus-embedded docs save with an .md extension. The suffix set matches
    // the app's open filter (MainWindow_File.cpp kOpenMdFilter) — .md,
    // .markdown and .txt all render as markdown.
    if (m_currentFile.isEmpty())
        return true;
    const QString suffix = QFileInfo(m_currentFile).suffix().toLower();
    return suffix == QStringLiteral("md")
        || suffix == QStringLiteral("markdown")
        || suffix == QStringLiteral("txt");
}

void Editor::setSpellCheckHighlight(int blockNumber, int start, int length)
{
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;
    QTextCursor cursor(document());
    cursor.setPosition(block.position() + start);
    cursor.setPosition(block.position() + start + length, QTextCursor::KeepAnchor);
    setTextCursor(cursor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    QTextCharFormat fmt;
    fmt.setBackground(SpellHighlighter::spellHighlightColor());
    sel.format = fmt;
    setExtraSelections({sel});
}

void Editor::clearSpellCheckHighlight()
{
    setExtraSelections({});
}

SpellHighlighter::WordHit Editor::misspelledWordAt(const QTextCursor &cursor) const
{
    if (!m_spellChecker || !m_spellChecker->isLoaded())
        return {};
    if (isCursorInFencedCodeBlock())
        return {};
    const int pos = cursor.positionInBlock();
    const QString line = cursor.block().text();
    for (const SpellHighlighter::WordHit &word : SpellHighlighter::scanWords(line)) {
        if (pos >= word.start && pos <= word.start + word.length
            && !m_spellChecker->checkWord(word.text))
            return word;
    }
    return {};
}

QString Editor::explanationAt(int blockNumber, int positionInBlock)
{
    if (!m_spellHighlighter)
        return {};

    // A finding's span covers [start, start + length]. Zero-length findings
    // (e.g. the consecutive-blank-lines check) are treated as covering the
    // whole line so the message shows wherever the line is hovered.
    const auto hitAt = [positionInBlock](const QVector<SpellHighlighter::GrammarHit> &hits)
        -> const SpellHighlighter::GrammarHit * {
        for (const SpellHighlighter::GrammarHit &hit : hits) {
            const bool covers = hit.length == 0
                || (positionInBlock >= hit.start
                    && positionInBlock <= hit.start + hit.length);
            if (covers)
                return &hit;
        }
        return nullptr;
    };

    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->markdownHitsInBlock(blockNumber)))
        return hit->message;
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->grammarIssuesInBlock(blockNumber)))
        return hit->message;
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->linkIssuesInBlock(blockNumber)))
        return QStringLiteral("Broken link: %1").arg(hit->message);
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->spellHitsInBlock(blockNumber))) {
        const QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (block.isValid()) {
            const QString word = block.text().mid(hit->start, hit->length);
            if (!word.isEmpty())
                return QStringLiteral("Misspelled word: %1").arg(word);
        }
    }
    return {};
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
    const QTextCursor cursor = cursorForPosition(event->pos());
    const QString tip = explanationAt(cursor.block().blockNumber(),
                                      cursor.positionInBlock());
    if (tip != m_activeTooltip) {
        m_activeTooltip = tip;
        if (tip.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(mapToGlobal(event->pos()), tip, this);
    }
    QTextEdit::mouseMoveEvent(event);
}

void Editor::leaveEvent(QEvent *event)
{
    m_activeTooltip.clear();
    QToolTip::hideText();
    QTextEdit::leaveEvent(event);
}

void Editor::invalidateEmojiIconCache()
{
    m_emojiIconCache.clear();
}

void Editor::centerCursor()
{
    QRect cr = cursorRect();
    if (cr.isNull())
        return;
    int vh = viewport()->height();
    int cy = cr.center().y() + verticalScrollBar()->value();
    int target = cy - vh / 2;
    target = qBound(0, target, verticalScrollBar()->maximum());
    verticalScrollBar()->setValue(target);
}

void Editor::setCenterContent(bool enabled, int width)
{
    m_centerContent = enabled;
    m_centerContentWidth = width;
    updateViewportMargins();
}

void Editor::applyLineWrap()
{
    QSettings settings;
    const bool enabled = settings.value(Preferences::EditorWrapEnabled, true).toBool();
    if (!enabled) {
        setLineWrapMode(QTextEdit::NoWrap);
    } else {
        const QString mode = settings.value(Preferences::EditorWrapMode,
                                             QStringLiteral("window")).toString();
        if (mode == QLatin1String("column")) {
            setLineWrapMode(QTextEdit::FixedColumnWidth);
            setLineWrapColumnOrWidth(settings.value(Preferences::EditorWrapColumn,
                                                     Preferences::DefaultEditorWrapColumn).toInt());
        } else {
            setLineWrapMode(QTextEdit::WidgetWidth);
        }
    }
    updateViewportMargins();
}

QMargins Editor::contentMargins() const
{
    return viewportMargins();
}

void Editor::setInsertActions(const QList<QAction *> &actions)
{
    m_insertActions = actions;
}

void Editor::setMermaidAction(QAction *action)
{
    m_mermaidAction = action;
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    if (m_centerContent && !m_inResize) {
        m_inResize = true;
        updateViewportMargins();
        m_inResize = false;
    }
    updateGutter();
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
    positionIssueSummaryPane();
}

void Editor::scrollContentsBy(int dx, int dy)
{
    QTextEdit::scrollContentsBy(dx, dy);
    // QWidget::scroll() (used by the base implementation) moves child widgets
    // of the viewport along with the content, which would drift the underline
    // overlay out of sync with the text. Re-anchor it to the viewport origin.
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
    positionIssueSummaryPane();
}

void Editor::updateViewportMargins()
{
    int gutterW = m_gutter ? m_gutter->width() : 0;
    if (!m_centerContent) {
        setViewportMargins(gutterW, 0, 0, 0);
        return;
    }
    // When wrapping at a fixed column the column count takes over as the
    // editor's effective max width: the centred region is the wrapped width
    // rather than m_centerContentWidth.
    int contentWidth = m_centerContentWidth;
    if (wrapAtColumnActive())
        contentWidth = qRound(wrapColumnPx());
    int available = width() - 2 * frameWidth() - gutterW;
    int scrollbarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    available -= scrollbarWidth;
    int margin = qMax(0, (available - contentWidth) / 2);
    setViewportMargins(margin + gutterW, 0, margin, 0);
}

bool Editor::wrapAtColumnActive() const
{
    QSettings settings;
    return settings.value(Preferences::EditorWrapEnabled, true).toBool()
        && settings.value(Preferences::EditorWrapMode,
                          QStringLiteral("window")).toString() == QLatin1String("column");
}

qreal Editor::wrapColumnPx() const
{
    const QFontMetrics fm = fontMetrics();
    const int col = QSettings().value(Preferences::EditorWrapColumn,
                                      Preferences::DefaultEditorWrapColumn).toInt();
    const qreal charWidth = fm.horizontalAdvance(QLatin1Char('M')) > 0
        ? fm.horizontalAdvance(QLatin1Char('M')) : fm.averageCharWidth();
    return charWidth * col;
}
