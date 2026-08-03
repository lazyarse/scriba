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
#include "LinkValidator.h"
#include "SpellChecker.h"
#include "StaticHelpers.h"
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

namespace {

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
    return QColor(0xd6, 0x40, 0x50);
}

QColor SpellHighlighter::grammarUnderlineColor()
{
    return QColor(0x00, 0xcc, 0x66);
}

QColor SpellHighlighter::linkUnderlineColor()
{
    return QColor(0xf0, 0x90, 0x00);
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
    // Schedule on contentsChange (not contentsChanged): rehighlight() and
    // other format-only work emit contentsChanged but no contentsChange, so
    // this prevents the checks from re-arming themselves into an infinite
    // loop.
    connect(document, &QTextDocument::contentsChange,
            this, &SpellHighlighter::scheduleSpellCheck);
    connect(document, &QTextDocument::contentsChange,
            this, &SpellHighlighter::scheduleGrammarLint);
}

GrammarLintWorker::GrammarLintWorker(GrammarChecker *checker)
    : m_checker(checker)
{
}

void GrammarLintWorker::doLint(quint64 generation, const QString &text)
{
    QList<GrammarChecker::Issue> issues;
    if (m_checker)
        issues = m_checker->check(text);
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
    if (m_lintThread) {
        m_lintThread->quit();
        m_lintThread->wait();
        // The thread has stopped; no code runs on it anymore.
        delete m_lintWorker;
        delete m_lintThread;
    }
}

void SpellHighlighter::ensureLintWorker()
{
    if (m_lintThread || !m_grammar)
        return;

    static const bool typesRegistered = []() {
        qRegisterMetaType<GrammarChecker::Issue>("GrammarChecker::Issue");
        qRegisterMetaType<QList<GrammarChecker::Issue>>("QList<GrammarChecker::Issue>");
        return true;
    }();
    Q_UNUSED(typesRegistered);

    m_lintThread = new QThread(this);
    m_lintThread->setObjectName(QStringLiteral("grammar-lint"));
    m_lintWorker = new GrammarLintWorker(m_grammar);
    m_lintWorker->moveToThread(m_lintThread);
    connect(m_lintWorker, &GrammarLintWorker::lintFinished,
            this, &SpellHighlighter::onLintFinished, Qt::QueuedConnection);
    m_lintThread->start();
}

void SpellHighlighter::setChecker(SpellChecker *checker)
{
    m_checker = checker;
}

void SpellHighlighter::setGrammarChecker(GrammarChecker *checker)
{
    m_grammar = checker;
    ensureLintWorker();
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

void SpellHighlighter::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    if (m_linkEnabled)
        runSpellCheck(); // relative targets resolve against a new base dir now
}

void SpellHighlighter::refresh()
{
    m_grammarIssues.clear();
    m_spellHits.clear();
    m_linkHits.clear();
    m_staleBlocks.clear();
    const bool spellActive = m_spellEnabled && m_checker && m_checker->isLoaded();
    if (spellActive || m_linkEnabled) {
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
    if ((!spellActive && !m_linkEnabled))
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
    if (!spellActive && !m_linkEnabled)
        return;

    // Reference definitions and heading slugs are collected over the whole
    // document first so usages on earlier lines can be validated against
    // definitions and headings that live on later lines.
    DocumentContext context;
    if (m_linkEnabled)
        context = collectDocumentContext();

    // Walk the whole document so fence/front-matter state stays consistent
    // even when only some blocks are stale (the state machine must advance
    // across the blocks in between).
    int state = 0;
    bool any = m_linkEnabled; // link hits are recomputed wholesale each pass
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        const int blockNumber = block.blockNumber();
        const BlockContext ctx = blockContext(blockNumber, block.text(), state);
        state = ctx.state;

        if (m_linkEnabled) {
            if (ctx.checkable)
                m_linkHits[blockNumber] = scanLinkHits(block.text(), context.refDefs,
                                                       context.headingSlugs);
            else
                m_linkHits.remove(blockNumber);
        }

        if (!m_staleBlocks.contains(blockNumber))
            continue;
        any = true;
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
    // Entries left over refer to blocks deleted since they were marked stale.
    m_staleBlocks.clear();
    if (any)
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
                // Block never checked (freshly loaded/replaced document whose
                // edit was not a user keystroke): check now so the first
                // paint shows the underlines.
                QVector<GrammarHit> hits;
                for (const WordHit &word : scanWords(text)) {
                    if (!m_checker->checkWord(word.text))
                        hits.append({word.start, word.length});
                }
                m_spellHits[blockNumber] = hits;
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
    if (!m_grammar || !m_grammarEnabled || !m_lintWorker) {
        rehighlight();
        return;
    }

    // Existing underlines stay visible while the fresh lint runs in the
    // background; they are replaced when the result arrives.
    const quint64 generation = ++m_lintGeneration;
    const QString text = document()->toPlainText();
    m_lintWorker->doLint(generation, text);
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

    const QString baseDir = m_currentFile.isEmpty()
        ? QString() : QFileInfo(m_currentFile).absolutePath();

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
            const QString dir = baseDir.isEmpty() ? QDir::currentPath() : baseDir;
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
