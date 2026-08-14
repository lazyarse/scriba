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
#include "SpellHighlighter.h"
#include "validation/LinkValidator.h"
#include "validation/MdLintEngine.h"
#include "prefs/Preferences.h"
#include "SpellChecker.h"
#include "StaticHelpers.h"
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace {

// The process-wide grammar-lint worker (see GrammarLintWorker's class comment):
// one thread, lazily started on first use, serving every SpellHighlighter.
// Requests are queued lambdas that carry their own checker shared_ptr, so no
// per-editor thread join is ever needed when a tab closes.
struct SharedLintWorker {
    QThread thread;
    GrammarLintWorker *worker = nullptr;

    SharedLintWorker()
    {
        static const bool typesRegistered = []() {
            qRegisterMetaType<GrammarChecker::Issue>("GrammarChecker::Issue");
            qRegisterMetaType<QList<GrammarChecker::Issue>>("QList<GrammarChecker::Issue>");
            return true;
        }();
        Q_UNUSED(typesRegistered);
        thread.setObjectName(QStringLiteral("grammar-lint"));
        worker = new GrammarLintWorker;
        worker->moveToThread(&thread);
        thread.start();
    }

    ~SharedLintWorker()
    {
        thread.quit();
        thread.wait();
        delete worker;
    }
};

SharedLintWorker &sharedLintWorker()
{
    static SharedLintWorker w;
    return w;
}

struct UnderlineColors {
    QColor spell;
    QColor grammar;
    QColor link;
    QColor markdown;
};

UnderlineColors loadUnderlineColors()
{
    // The current defaults when "Override underline colors" is off; stored
    // values when it is on.
    static const QColor spellDefault(0xd6, 0x40, 0x50);
    static const QColor grammarDefault(0x00, 0xcc, 0x66);
    static const QColor linkDefault(0xf0, 0x90, 0x00);
    static const QColor markdownDefault(0x3b, 0x82, 0xf6);
    UnderlineColors c;
    if (!QSettings().value(Preferences::UnderlineColorOverride, false).toBool()) {
        c.spell = spellDefault;
        c.grammar = grammarDefault;
        c.link = linkDefault;
        c.markdown = markdownDefault;
        return c;
    }
    c.spell = QColor(QSettings().value(Preferences::SpellUnderlineColor,
        spellDefault.name()).toString());
    c.grammar = QColor(QSettings().value(Preferences::GrammarUnderlineColor,
        grammarDefault.name()).toString());
    c.link = QColor(QSettings().value(Preferences::LinkUnderlineColor,
        linkDefault.name()).toString());
    c.markdown = QColor(QSettings().value(Preferences::MarkdownUnderlineColor,
        markdownDefault.name()).toString());
    return c;
}

UnderlineColors &underlineColors()
{
    static UnderlineColors colors = loadUnderlineColors();
    return colors;
}

QTextCharFormat spellFormat()
{
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    fmt.setUnderlineColor(SpellHighlighter::spellUnderlineColor());
    return fmt;
}

QTextCharFormat grammarFormat()
{
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    fmt.setUnderlineColor(SpellHighlighter::grammarUnderlineColor());
    return fmt;
}

// A separator ends a word: space, punctuation, newline, hyphen (scanWords()
// splits on hyphens anyway), etc. Apostrophes stay inside words.
bool isWordSeparator(QChar c)
{
    return !c.isLetterOrNumber() && c != QLatin1Char('\'') && c != QChar(0x2019);
}

// Block context for spell checking: 0 = plain, 1 = fenced code, 2 = front
// matter. Shared by highlightBlock() (which gets the previous state from the
// document) and runSpellCheck() (which tracks the state itself while walking
// the blocks).
struct BlockContext {
    int state = 0;
    bool checkable = true;
};

BlockContext blockContext(int blockNumber, const QString &text, int previousState)
{
    static const QRegularExpression fenceRe(R"(^\s*(?:```+|~~~+))");
    static const QRegularExpression frontMatterRe(R"(^\s*---\s*$)");

    const bool opener = fenceRe.match(text).hasMatch();
    const bool isFrontMatterBoundary = frontMatterRe.match(text).hasMatch();

    BlockContext ctx;
    if (previousState == 1) {
        ctx.state = opener ? 0 : 1;
    } else if (previousState == 2) {
        ctx.state = isFrontMatterBoundary ? 0 : 2;
    } else if (blockNumber == 0 && isFrontMatterBoundary) {
        ctx.state = 2;
    } else if (opener) {
        ctx.state = 1;
    }
    ctx.checkable = previousState != 1 && previousState != 2 && !opener
        && !(blockNumber == 0 && isFrontMatterBoundary);
    return ctx;
}

} // namespace

QColor SpellHighlighter::spellUnderlineColor()
{
    return underlineColors().spell;
}

QColor SpellHighlighter::grammarUnderlineColor()
{
    return underlineColors().grammar;
}

QColor SpellHighlighter::linkUnderlineColor()
{
    return underlineColors().link;
}

QColor SpellHighlighter::markdownUnderlineColor()
{
    return underlineColors().markdown;
}

void SpellHighlighter::reloadUnderlineColors()
{
    underlineColors() = loadUnderlineColors();
}

QColor SpellHighlighter::spellHighlightColor()
{
    // Translucent amber: reads on both light and dark editor themes.
    return QColor(0xff, 0xd7, 0x00, 0x99);
}

SpellHighlighter::SpellHighlighter(QTextDocument *document, QObject *parent)
    : QSyntaxHighlighter(document)
    , m_lintTimer(new QTimer(this))
    , m_spellTimer(new QTimer(this))
    , m_lastBlockCount(document->blockCount())
{
    if (parent)
        setParent(parent);
    m_lintTimer->setSingleShot(true);
    m_lintTimer->setInterval(Debounce::SpellCheck);
    connect(m_lintTimer, &QTimer::timeout, this, &SpellHighlighter::runGrammarLint);
    m_spellTimer->setSingleShot(true);
    m_spellTimer->setInterval(Debounce::SpellCheck);
    connect(m_spellTimer, &QTimer::timeout, this, &SpellHighlighter::runSpellCheck);
    m_chunkTimer = new QTimer(this);
    m_chunkTimer->setSingleShot(true);
    m_chunkTimer->setInterval(0);
    connect(m_chunkTimer, &QTimer::timeout, this, &SpellHighlighter::continueSpellScan);
    // Schedule on contentsChange (not contentsChanged): rehighlight() and
    // other format-only work emit contentsChanged but no contentsChange, so
    // this prevents the checks from re-arming themselves into an infinite
    // loop.
    connect(document, &QTextDocument::contentsChange,
            this, &SpellHighlighter::scheduleSpellCheck);
    connect(document, &QTextDocument::contentsChange,
            this, &SpellHighlighter::scheduleGrammarLint);
}

void GrammarLintWorker::doLint(quint64 generation,
                               std::shared_ptr<GrammarChecker> checker,
                               const QString &text)
{
    QList<GrammarChecker::Issue> issues;
    if (checker)
        issues = checker->check(text);
    emit lintFinished(generation, text, issues);
}

SpellHighlighter::~SpellHighlighter()
{
    // ~QSyntaxHighlighter detaches the document (setDocument(nullptr) →
    // finishEdit()), which re-emits contentsChange; drop our connections first
    // so the slots are never invoked on a dying object (the QObject
    // auto-disconnect would only happen in ~QObject, which runs later).
    if (QTextDocument *doc = document()) {
        disconnect(doc, &QTextDocument::contentsChange,
                   this, &SpellHighlighter::scheduleSpellCheck);
        disconnect(doc, &QTextDocument::contentsChange,
                   this, &SpellHighlighter::scheduleGrammarLint);
    }
    // The lint worker is shared process-wide, so there is no thread to join
    // here: any in-flight grammar check keeps the checker alive via its own
    // shared_ptr and its result is dropped when this object's connection to
    // onLintFinished() disconnects. Closing a tab never blocks on a lint.
}

void SpellHighlighter::setChecker(SpellChecker *checker)
{
    m_checker = checker;
}

void SpellHighlighter::setGrammarChecker(std::shared_ptr<GrammarChecker> checker)
{
    m_grammar = std::move(checker);
    if (m_grammarLintConnection)
        disconnect(m_grammarLintConnection);
    m_grammarLintConnection = {};
    if (!m_grammar)
        return;
    // The worker is shared, so the connection is to the singleton's signal; it
    // delivers lintFinished() back to this highlighter on the main thread and
    // auto-disconnects (dropping any queued result) when this object dies.
    m_grammarLintConnection = connect(
        sharedLintWorker().worker, &GrammarLintWorker::lintFinished,
        this, &SpellHighlighter::onLintFinished, Qt::QueuedConnection);
}

void SpellHighlighter::setSpellCheckingEnabled(bool enabled)
{
    m_spellEnabled = enabled;
    if (!enabled) {
        m_spellHits.clear();
        m_staleBlocks.clear();
        m_spellTimer->stop();
    }
    rehighlight();
}

void SpellHighlighter::setGrammarCheckingEnabled(bool enabled)
{
    m_grammarEnabled = enabled;
    m_grammarIssues.clear();
    ++m_lintGeneration; // drop any in-flight lint result
    rehighlight();
    if (enabled)
        m_lintTimer->start();
}

void SpellHighlighter::setLinkCheckingEnabled(bool enabled)
{
    m_linkEnabled = enabled;
    m_linkHits.clear();
    if (enabled)
        runSpellCheck(); // recompute so fresh underlines appear immediately
    else
        emit spellHitsChanged(); // clear any painted underlines
}

void SpellHighlighter::setMarkdownCheckingEnabled(bool enabled)
{
    m_markdownEnabled = enabled;
    m_markdownHits.clear();
    if (enabled)
        runSpellCheck(); // recompute so fresh underlines appear immediately
    else
        emit spellHitsChanged(); // clear any painted underlines
}

void SpellHighlighter::setMarkdownConfig(const MdLintConfig &config)
{
    m_markdownConfig = config;
    m_markdownHits.clear();
    if (m_markdownEnabled)
        runSpellCheck(); // recompute so underlines follow the new selection
    else
        emit spellHitsChanged(); // clear any painted underlines
}

void SpellHighlighter::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    if (m_linkEnabled)
        runSpellCheck(); // relative targets resolve against a new base dir now
}

void SpellHighlighter::setFallbackLinkBaseDir(const QString &dir)
{
    if (m_fallbackLinkBaseDir == dir)
        return;
    m_fallbackLinkBaseDir = dir;
    if (m_linkEnabled)
        runSpellCheck(); // relative targets resolve against the new base now
}

void SpellHighlighter::setForceSyncChecks(bool force)
{
    m_forceSyncChecks = force;
}

bool SpellHighlighter::largeDocument() const
{
    return document() && document()->blockCount() > kLargeDocBlocks;
}

void SpellHighlighter::refresh()
{
    m_grammarIssues.clear();
    m_spellHits.clear();
    m_linkHits.clear();
    m_markdownHits.clear();
    m_staleBlocks.clear();
    const bool spellActive = m_spellEnabled && m_checker && m_checker->isLoaded();
    if (spellActive || m_linkEnabled || m_markdownEnabled) {
        for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next())
            m_staleBlocks.insert(block.blockNumber());
        runSpellCheck();
    }
    rehighlight();
    if (m_grammar && m_grammarEnabled)
        m_lintTimer->start();
}

void SpellHighlighter::scheduleSpellCheck(int position, int charsRemoved, int charsAdded)
{
    const bool spellActive = m_spellEnabled && m_checker && m_checker->isLoaded();
    if ((!spellActive && !m_linkEnabled && !m_markdownEnabled))
        return;
    // Format-only changes (highlighting, block-state updates) emit
    // contentsChange with (0,0) — the edits' own reformats. Skip those, or
    // the checks would re-arm themselves into an endless loop.
    if (charsRemoved == 0 && charsAdded == 0)
        return;

    // A removal-only change spanning the whole document is a load or replace
    // (setPlainText, clear, select-all-delete) rather than typing; likewise a
    // multi-character insertion at position 0 fills an empty document (a
    // paste, or the contentsChange QTextDocumentLayout emits when it is
    // attached to a bare document). Both are loads: check the new content
    // right away. (Single-keystroke edits into an empty document have
    // charsAdded == 1 and stay on the debounce path.)
    const bool fullReplace = position == 0 && charsAdded == 0
        && charsRemoved >= document()->characterCount() - 1;
    const bool wholeDocInsert = position == 0 && charsRemoved == 0 && charsAdded > 1;

    // Block insertions/removals renumber every following block, so their
    // cached hits would end up on the wrong lines: re-check everything from
    // the first affected block on.
    const bool structureChanged = document()->blockCount() != m_lastBlockCount;
    m_lastBlockCount = document()->blockCount();

    QTextBlock first = document()->findBlock(position);
    QTextBlock last = structureChanged
        ? document()->lastBlock()
        : document()->findBlock(position + qMax(charsAdded, charsRemoved));
    // The edit may end exactly at the end of the document, where no block
    // exists; the last block is then the affected one.
    if (!last.isValid())
        last = document()->lastBlock();
    if (!first.isValid() || !last.isValid())
        return;
    const int lastNumber = last.blockNumber();
    for (QTextBlock block = first; block.isValid() && block.blockNumber() <= lastNumber; block = block.next()) {
        m_staleBlocks.insert(block.blockNumber());
        // Drop cached hits so no underlines linger while the word is being
        // typed; runSpellCheck() repopulates them.
        m_spellHits.remove(block.blockNumber());
    }

    if (fullReplace || wholeDocInsert) {
        runSpellCheck();
        return;
    }

    // Check immediately when the edit itself completes a word — the inserted
    // character is a separator (space, punctuation, newline, hyphen, ...).
    // Note the character *after* the change is not a signal: at a block end
    // it is the paragraph separator, so mid-word typing at the end of a line
    // would be flagged on every keystroke. Mid-word typing and pure
    // deletions (the user may be editing an incomplete word) go to the
    // debounce timer instead.
    const QChar lastInserted = charsAdded > 0
        ? document()->characterAt(position + charsAdded - 1) : QChar();
    if (charsAdded > 0 && isWordSeparator(lastInserted))
        runSpellCheck();
    else
        m_spellTimer->start();
}

void SpellHighlighter::runSpellCheck()
{
    const bool spellActive = m_spellEnabled && m_checker && m_checker->isLoaded();
    if (!spellActive && !m_linkEnabled && !m_markdownEnabled)
        return;

    // Reference definitions and heading slugs are collected over the whole
    // document first so usages on earlier lines can be validated against
    // definitions and headings that live on later lines. The document context
    // is cached in m_scanContext for the chunked continuation.
    m_scanContext = DocumentContext();
    if (m_linkEnabled)
        m_scanContext = collectDocumentContext();

    // Markdown-consistency findings are document-global (a duplicate heading
    // or a level skip depends on headings on other lines), so scan the whole
    // document once and bucket the results by block.
    if (m_markdownEnabled) {
        QStringList lines;
        for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next())
            lines.append(block.text());
        m_markdownHits.clear();
        // Issue::line is 1-based and lines map 1:1 to blocks; Issue::col is
        // 1-based and GrammarHit::start is 0-based within the block.
        const auto mdIssues = MdLintEngine::lint(lines.join(QLatin1Char('\n')),
                                                 m_markdownConfig);
        for (const auto &issue : mdIssues) {
            const int blockNumber = issue.line - 1;
            if (blockNumber < 0 || blockNumber >= lines.size())
                continue;
            m_markdownHits[blockNumber].append({issue.col - 1, issue.length, issue.detail, {}});
        }
    }

    // Start (or restart) the per-block pass. On large documents the walk is
    // deferred into chunks on a 0 ms timer so the UI thread never blocks on
    // one monolithic pass; validation scans (setForceSyncChecks) keep it
    // synchronous.
    m_scanState = 0;
    m_scanAny = m_linkEnabled || m_markdownEnabled; // these recompute wholesale each pass
    m_scanBlockNumber = 0;
    m_scanChunked = !m_forceSyncChecks && largeDocument();
    continueSpellScan();
}

void SpellHighlighter::continueSpellScan()
{
    const bool spellActive = m_spellEnabled && m_checker && m_checker->isLoaded();
    QTextBlock block = document()->firstBlock();
    for (int skip = m_scanBlockNumber; skip > 0 && block.isValid(); --skip)
        block = block.next();

    // Walk a chunk of blocks so fence/front-matter state stays consistent
    // across the whole document (the state machine must advance across every
    // block in between, stale or not).
    int processed = 0;
    for (; block.isValid(); block = block.next()) {
        const int blockNumber = block.blockNumber();
        const BlockContext ctx = blockContext(blockNumber, block.text(), m_scanState);
        m_scanState = ctx.state;

        if (m_linkEnabled) {
            if (ctx.checkable)
                m_linkHits[blockNumber] = scanLinkHits(block.text(), m_scanContext.refDefs,
                                                       m_scanContext.headingSlugs);
            else
                m_linkHits.remove(blockNumber);
        }

        if (m_staleBlocks.contains(blockNumber)) {
            m_scanAny = true;
            m_staleBlocks.remove(blockNumber);
            if (!ctx.checkable) {
                m_spellHits.remove(blockNumber);
                continue;
            }
            if (spellActive) {
                QVector<GrammarHit> hits;
                for (const WordHit &word : scanWords(block.text())) {
                    if (!m_checker->checkWord(word.text))
                        hits.append({word.start, word.length});
                }
                m_spellHits[blockNumber] = hits;
            }
        }

        if (m_scanChunked && ++processed >= kSpellScanChunkBlocks) {
            const QTextBlock next = block.next();
            m_scanBlockNumber = next.isValid() ? next.blockNumber()
                                               : block.blockNumber() + 1;
            if (next.isValid()) {
                m_chunkTimer->start();
                return;
            }
            break; // chunk boundary landed exactly on the last block
        }
    }

    // Document exhausted: the pass is complete. Entries left over in
    // m_staleBlocks refer to blocks deleted since they were marked stale.
    m_scanBlockNumber = 0;
    m_staleBlocks.clear();
    if (m_scanAny)
        emit spellHitsChanged();
}

QVector<QPair<int, int>> SpellHighlighter::protectedRanges(const QString &line)
{
    static const QRegularExpression re(
        "`[^`\\n]*`"                                    // inline code
        "|\\$\\$[^\\n]*|\\$[^$\\n]*\\$"                // math
        "|(?:https?|ftp)://[^\\s<>\\)\\]]+"             // raw URLs
        "|www\\.[^\\s<>\\)\\]]+"                        // bare www URLs
        "|<[^>\\n]*>"                                   // html tags + autolinks
        "|:[\\w+_-]+:"                                  // emoji shortcodes
        "|\\]\\[[^\\]\\n]+\\]"                          // ref link targets [..][ref]
        "|\\[\\^[^\\]\\n]+\\]"                          // footnotes [^..]
        "|\\]\\([^()\\s\\n]+\\)");                      // link/image targets ](url)
    QVector<QPair<int, int>> ranges;
    QRegularExpressionMatchIterator it = re.globalMatch(line);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        ranges.append({m.capturedStart(), m.capturedLength()});
    }
    return ranges;
}

QList<SpellHighlighter::WordHit> SpellHighlighter::scanWords(const QString &line)
{
    static const QRegularExpression wordRe(
        "[A-Za-z\\x{00C0}-\\x{024F}]+(?:['\\x{2019}][A-Za-z\\x{00C0}-\\x{024F}]+)*");
    const QVector<QPair<int, int>> spans = protectedRanges(line);
    QList<WordHit> hits;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(line);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        const int start = m.capturedStart();
        const int len = m.capturedLength();
        bool covered = false;
        for (const auto &span : spans) {
            if (start >= span.first && start < span.first + span.second) {
                covered = true;
                break;
            }
        }
        if (covered)
            continue;
        hits.append({m.captured(), start, len});
    }
    return hits;
}

QVector<SpellHighlighter::SpellIssue>
SpellHighlighter::scanDocument(QTextDocument *document, SpellChecker *checker)
{
    QVector<SpellIssue> issues;
    if (!document || !checker || !checker->isLoaded())
        return issues;

    int state = 0;
    for (QTextBlock block = document->firstBlock(); block.isValid(); block = block.next()) {
        const BlockContext ctx = blockContext(block.blockNumber(), block.text(), state);
        state = ctx.state;
        if (!ctx.checkable)
            continue;
        for (const WordHit &word : scanWords(block.text())) {
            if (!checker->checkWord(word.text))
                issues.append({block.blockNumber(), word.start, word.length, word.text});
        }
    }
    return issues;
}

QVector<SpellHighlighter::LinkHit>
SpellHighlighter::scanLinkIssues(const QString &text, const QString &baseDir)
{
    QTextDocument doc;
    doc.setPlainText(text);
    // Attaching a highlighter and setting the current file runs the same full
    // link pass the underlines use (setCurrentFile triggers runSpellCheck(),
    // whose link half needs no spell checker). Relative targets resolve
    // against baseDir via the synthetic file path.
    SpellHighlighter hl(&doc);
    hl.setForceSyncChecks(true); // validation must read finished caches immediately
    hl.setCurrentFile(baseDir.isEmpty() ? QString()
                                        : QDir(baseDir).filePath(QStringLiteral("__validation__.md")));
    QVector<LinkHit> hits;
    for (int i = 0; i < doc.blockCount(); ++i) {
        const QVector<GrammarHit> blockHits = hl.linkIssuesInBlock(i);
        for (const auto &h : blockHits)
            hits.append({i + 1, h.start + 1, h.length, h.message});
    }
    return hits;
}

void SpellHighlighter::highlightBlock(const QString &text)
{
    const int blockNumber = currentBlock().blockNumber();
    const int previousState = previousBlockState();
    const BlockContext ctx = blockContext(blockNumber, text, previousState);
    setCurrentBlockState(ctx.state);

    if (!ctx.checkable) {
        setFormat(0, text.length(), QTextCharFormat());
        m_spellHits.remove(blockNumber);
        m_linkHits.remove(blockNumber);
        m_staleBlocks.remove(blockNumber);
        return;
    }

    bool applied = false;
    if (m_grammarEnabled && m_grammar) {
        const QVector<GrammarHit> issues = m_grammarIssues.value(blockNumber);
        for (const auto &issue : issues) {
            const int len = qMin(issue.length, text.length() - issue.start);
            if (issue.start >= 0 && len > 0) {
                applied = true;
            }
        }
    }

    if (m_spellEnabled && m_checker && m_checker->isLoaded()) {
        if (!m_staleBlocks.contains(blockNumber)) {
            if (!m_spellHits.contains(blockNumber)) {
                if (largeDocument()) {
                    // Block never checked (freshly loaded/replaced document
                    // whose edit was not a user keystroke). On a large document
                    // the eager per-word check would block the paint pass for
                    // every block, so defer: mark the block stale and let the
                    // chunked scan in runSpellCheck() fill the hits
                    // asynchronously — underlines appear progressively.
                    m_staleBlocks.insert(blockNumber);
                } else {
                    // Block never checked: check now so the first paint shows
                    // the underlines.
                    QVector<GrammarHit> hits;
                    for (const WordHit &word : scanWords(text)) {
                        if (!m_checker->checkWord(word.text))
                            hits.append({word.start, word.length});
                    }
                    m_spellHits[blockNumber] = hits;
                }
            }
            if (!m_spellHits.value(blockNumber).isEmpty())
                applied = true;
        }
    } else {
        m_spellHits.remove(blockNumber);
        m_staleBlocks.remove(blockNumber);
    }

    if (!applied)
        setFormat(0, text.length(), QTextCharFormat());
}

void SpellHighlighter::scheduleGrammarLint(int charsRemoved, int charsAdded)
{
    if (!m_grammar || !m_grammarEnabled)
        return;
    // Format-only changes (highlighting, block-state updates) emit
    // contentsChange with (0,0). Only real text edits need a re-lint —
    // skipping these also prevents the lint's own rehighlight() from
    // re-arming the timer into an endless loop.
    if (charsRemoved == 0 && charsAdded == 0)
        return;
    m_lintTimer->start();
}

void SpellHighlighter::runGrammarLint()
{
    if (!m_grammar || !m_grammarEnabled) {
        rehighlight();
        return;
    }
    // Large documents skip the whole-document lint entirely: the check is
    // O(n) per line and even on a background thread a 2 MB file would take
    // seconds, with underlines stale long before they ever paint. Grammar
    // checking on multi-thousand-block files is a Validation Report job.
    if (largeDocument())
        return;

    // Existing underlines stay visible while the fresh lint runs in the
    // background; they are replaced when the result arrives.
    const quint64 generation = ++m_lintGeneration;
    const QString text = document()->toPlainText();
    auto &shared = sharedLintWorker();
    GrammarLintWorker *worker = shared.worker;
    std::shared_ptr<GrammarChecker> checker = m_grammar;
    // Queued functor invocation: runs doLint() on the worker thread, keeping
    // the checker alive for the duration of the request even if this
    // highlighter (and editor) is destroyed in the meantime.
    QMetaObject::invokeMethod(worker, [generation, checker, text, worker]() {
        worker->doLint(generation, checker, text);
    }, Qt::QueuedConnection);
}

void SpellHighlighter::onLintFinished(quint64 generation, const QString &text,
                                      const QList<GrammarChecker::Issue> &issues)
{
    if (generation != m_lintGeneration)
        return; // superseded by a newer edit or a disable
    if (text != document()->toPlainText())
        return; // content changed mid-lint; the edit already scheduled a fresh one

    m_grammarIssues.clear();
    int blockStart = 0;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        const int blockLen = block.text().length();
        for (const auto &issue : issues) {
            if (issue.start >= blockStart && issue.start < blockStart + blockLen) {
                m_grammarIssues[block.blockNumber()].append({issue.start - blockStart,
                                                             issue.length, issue.message,
                                                             issue.suggestions});
            }
        }
        blockStart += blockLen + 1;
    }
    rehighlight();
    emit spellHitsChanged(); // the grammar cache was refreshed
}

QVector<SpellHighlighter::GrammarHit> SpellHighlighter::grammarIssuesInBlock(int blockNumber) const
{
    return m_grammarIssues.value(blockNumber);
}

QVector<SpellHighlighter::GrammarHit> SpellHighlighter::spellHitsInBlock(int blockNumber) const
{
    return m_spellHits.value(blockNumber);
}

QVector<SpellHighlighter::GrammarHit> SpellHighlighter::linkIssuesInBlock(int blockNumber) const
{
    return m_linkHits.value(blockNumber);
}

QVector<SpellHighlighter::GrammarHit> SpellHighlighter::markdownHitsInBlock(int blockNumber) const
{
    return m_markdownHits.value(blockNumber);
}

SpellHighlighter::IssueCounts SpellHighlighter::counts() const
{
    IssueCounts c;
    for (auto it = m_spellHits.constBegin(); it != m_spellHits.constEnd(); ++it)
        c.spelling += it.value().size();
    for (auto it = m_grammarIssues.constBegin(); it != m_grammarIssues.constEnd(); ++it)
        c.grammar += it.value().size();
    for (auto it = m_linkHits.constBegin(); it != m_linkHits.constEnd(); ++it)
        c.links += it.value().size();
    for (auto it = m_markdownHits.constBegin(); it != m_markdownHits.constEnd(); ++it)
        c.markdown += it.value().size();
    return c;
}

SpellHighlighter::DocumentContext SpellHighlighter::collectDocumentContext() const
{
    // [name]: target  — definition lines for reference-style links.
    static const QRegularExpression defRe(
        R"(^\s*\[([^\]]+)\]:\s*(<[^>\s]+>|[^\s]+))");
    DocumentContext context;
    QStringList lines;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next())
        lines << block.text();
    int state = 0;
    for (int i = 0; i < lines.size(); ++i) {
        const BlockContext ctx = blockContext(i, lines.at(i), state);
        state = ctx.state;
        if (!ctx.checkable)
            continue;
        const QRegularExpressionMatch m = defRe.match(lines.at(i));
        if (m.hasMatch())
            context.refDefs.insert(m.captured(1));
    }
    context.headingSlugs = headingSlugsFromLines(lines);
    return context;
}

QSet<QString> SpellHighlighter::headingSlugsFromLines(const QStringList &lines) const
{
    // ATX headings `# Title` and setext headings (`Title` + `===`/`---`
    // underline), fenced-code and front-matter aware — mirroring what md4c
    // renders and the preview's generateHeadingIds() then slugs.
    static const QRegularExpression atxRe(R"(^#{1,6}\s+(.+))");
    static const QRegularExpression setextRe(R"(^(?:={3,}|-{3,})\s*$)");
    QSet<QString> slugs;
    int state = 0;
    bool prevCheckable = false;
    bool prevWasHeading = false;
    QString prevText;
    int lineIndex = 0;
    for (const QString &line : lines) {
        const BlockContext ctx = blockContext(lineIndex++, line, state);
        state = ctx.state;
        if (!ctx.checkable) {
            prevCheckable = false;
            prevWasHeading = false;
            prevText.clear();
            continue;
        }
        const QRegularExpressionMatch atx = atxRe.match(line);
        if (atx.hasMatch()) {
            // Strip the closing hashes of `# Title ##` like md4c does.
            QString title = atx.captured(1).trimmed();
            while (title.endsWith(QLatin1Char('#')))
                title.chop(1);
            LinkValidator::addHeadingSlugs(slugs, title.trimmed());
            prevCheckable = true;
            prevWasHeading = true;
            prevText = line;
            continue;
        }
        if (setextRe.match(line).hasMatch()) {
            if (prevCheckable && !prevWasHeading)
                LinkValidator::addHeadingSlugs(slugs, prevText.trimmed());
            prevCheckable = false;
            prevWasHeading = false;
            prevText.clear();
            continue;
        }
        prevCheckable = true;
        prevWasHeading = false;
        prevText = line;
    }
    return slugs;
}

QSet<QString> SpellHighlighter::crossDocSlugs(const QString &absolutePath)
{
    const QFileInfo fi(absolutePath);
    if (!fi.exists() || !fi.isFile())
        return {};
    const QDateTime mtime = fi.lastModified();
    const qint64 size = fi.size();
    const auto it = m_anchorCache.constFind(absolutePath);
    if (it != m_anchorCache.constEnd() && it->mtime == mtime && it->size == size)
        return it->slugs;
    if (m_anchorCache.size() >= 32)
        m_anchorCache.clear();
    QSet<QString> slugs;
    QFile file(absolutePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        slugs = headingSlugsFromLines(QString::fromUtf8(file.readAll()).split(QLatin1Char('\n')));
    m_anchorCache.insert(absolutePath, {mtime, size, slugs});
    return slugs;
}

QVector<SpellHighlighter::GrammarHit>
SpellHighlighter::scanLinkHits(const QString &line, const QSet<QString> &refDefs,
                               const QSet<QString> &currentSlugs)
{
    static const QRegularExpression targetRe(R"(\]\(([^()\s\n]+)\))"); // [t](target)
    static const QRegularExpression refUsageRe(R"(\]\[([^\]\n]+)\])"); // [t][ref]
    static const QRegularExpression urlRe(
        R"((?:https?|ftp)://[^\s<>\\)\]]+|www\.[^\s<>\\)\]]+)"); // raw URLs
    static const QRegularExpression defRe(
        R"(^\s*\[([^\]]+)\]:\s*(<[^>\s]+>|[^\s]+))"); // [name]: target
    // Markdown spans that must not be treated as links: inline code, math,
    // emoji shortcodes, HTML tags, footnotes.
    static const QRegularExpression skipRe(
        "`[^`\\n]*`|\\$\\$[^\\n]*|\\$[^$\\n]*\\$|:[\\w+_-]+:|<[^>\\n]*>|\\[\\^[^\\]\\n]+\\]");

    const QString baseDir = !m_currentFile.isEmpty()
        ? QFileInfo(m_currentFile).absolutePath()
        : m_fallbackLinkBaseDir;

    QVector<GrammarHit> hits;
    auto classify = [&](const QString &target) {
        switch (LinkValidator::validateTarget(target, baseDir)) {
        case LinkValidator::Status::FileNotFound:
            return QStringLiteral("File not found: ") + target;
        case LinkValidator::Status::MalformedUrl:
            return QStringLiteral("Malformed URL: ") + target;
        case LinkValidator::Status::Valid:
            return QString();
        }
        return QString();
    };
    auto skipSpans = [&]() {
        QVector<QPair<int, int>> spans;
        QRegularExpressionMatchIterator it = skipRe.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            spans.append({m.capturedStart(), m.capturedLength()});
        }
        return spans;
    }();
    auto inSpan = [&](int start, int length) {
        for (const auto &span : skipSpans) {
            if (start >= span.first && start < span.first + span.second)
                return true;
            if (length > 0 && start + length > span.first && start < span.first + span.second)
                return true;
        }
        return false;
    };

    // Link/image target spans, to exclude the raw-URL scan from re-reporting
    // targets that were already classified above (e.g. `[text](https://x)`).
    QVector<QPair<int, int>> targetSpans;
    QRegularExpressionMatchIterator targetIt = targetRe.globalMatch(line);
    while (targetIt.hasNext()) {
        const QRegularExpressionMatch m = targetIt.next();
        targetSpans.append({m.capturedStart(0), m.capturedLength(0)});
    }

    // Reference definitions: the target itself may be a broken file/URL.
    const QRegularExpressionMatch def = defRe.match(line);
    if (def.hasMatch()) {
        const int targetStart = def.capturedStart(2);
        const QString message = classify(def.captured(2));
        if (!message.isEmpty())
            hits.append({targetStart, int(def.capturedLength(2)), message, {}});
    }

    // Inline and image link targets: [text](target), ![alt](target).
    static const QRegularExpression webTargetRe(
        R"(^(?:https?|ftp)://|^www\.)", QRegularExpression::CaseInsensitiveOption);
    auto slugReference = [](const QString &frag) {
        return LinkValidator::headingSlug(QUrl::fromPercentEncoding(frag.toUtf8()));
    };
    // Missing-anchor message for target.file#frag. absFile is the resolved
    // absolute path for cross-document anchors; empty for same-document ones
    // (checked against the current document's headings).
    auto headingMessage = [&](const QString &frag, const QString &absFile) {
        const QString slug = slugReference(frag);
        const QSet<QString> pool = absFile.isEmpty() ? currentSlugs : crossDocSlugs(absFile);
        if (slug.isEmpty() || !pool.contains(slug))
            return QStringLiteral("Heading not found: #") + frag;
        return QString();
    };
    for (const auto &span : targetSpans) {
        const int targetStart = span.first + 2; // past the `](`
        const int targetLength = span.second - 3; // minus `](` and the closing `)`
        if (inSpan(targetStart, targetLength))
            continue;
        const QString target = line.mid(targetStart, targetLength);
        const int hash = target.indexOf(QLatin1Char('#'));
        if (hash < 0) {
            const QString message = classify(target);
            if (!message.isEmpty())
                hits.append({targetStart, targetLength, message, {}});
            continue;
        }
        const QString filePart = target.left(hash);
        const QString frag = target.mid(hash + 1);
        if (frag.isEmpty())
            continue; // `path#` — not an anchor navigation, nothing to check
        if (filePart.isEmpty()) {
            const QString message = headingMessage(frag, {});
            if (!message.isEmpty()) {
                const int fragStart = targetStart + hash;
                hits.append({fragStart, targetLength - hash, message, {}});
            }
            continue;
        }
        if (webTargetRe.match(filePart).hasMatch()) {
            const QString message = classify(filePart);
            if (!message.isEmpty())
                hits.append({targetStart, targetLength, message, {}});
            continue;
        }
        // A file target with an anchor: the file part is what makes the link
        // resolvable, and its headings decide whether the anchor exists.
        const QString fileMessage = classify(filePart);
        if (!fileMessage.isEmpty()) {
            hits.append({targetStart, targetLength, fileMessage, {}});
            continue;
        }
        QString resolved = filePart;
        if (resolved.startsWith(QLatin1Char('~'))) {
            resolved = QDir::homePath() + resolved.mid(1);
        } else if (!QFileInfo(filePart).isAbsolute()) {
            const QString dir = !baseDir.isEmpty() ? baseDir
                                : !m_fallbackLinkBaseDir.isEmpty() ? m_fallbackLinkBaseDir
                                                                   : QDir::currentPath();
            resolved = QDir(dir).absoluteFilePath(resolved);
        }
        const QString message = headingMessage(frag, resolved);
        if (!message.isEmpty()) {
            const int fragStart = targetStart + hash;
            hits.append({fragStart, targetLength - hash, message, {}});
        }
    }

    // Reference usages without a matching definition.
    QRegularExpressionMatchIterator it = refUsageRe.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (inSpan(m.capturedStart(1), m.capturedLength(1)))
            continue;
        if (refDefs.contains(m.captured(1)))
            continue;
        hits.append({int(m.capturedStart(1)), int(m.capturedLength(1)),
                     QStringLiteral("Missing reference definition: [") + m.captured(1)
                         + QStringLiteral("]"),
                     {}});
    }

    // Raw URLs in plain text (md4c autolinks them). Targets and definition
    // targets are excluded — they were already classified above.
    it = urlRe.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (inSpan(m.capturedStart(), m.capturedLength()))
            continue;
        bool insideTarget = false;
        for (const auto &span : targetSpans) {
            if (m.capturedStart() >= span.first && m.capturedStart() < span.first + span.second) {
                insideTarget = true;
                break;
            }
        }
        if (insideTarget)
            continue;
        if (def.hasMatch() && m.capturedStart() >= def.capturedStart(2)
            && m.capturedStart() < def.capturedEnd(2))
            continue;
        const QString message = classify(m.captured());
        if (!message.isEmpty())
            hits.append({int(m.capturedStart()), int(m.capturedLength()), message, {}});
    }

    return hits;
}
