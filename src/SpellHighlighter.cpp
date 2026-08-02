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
#include "SpellChecker.h"
#include <QColor>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTimer>

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

} // namespace

QColor SpellHighlighter::spellUnderlineColor()
{
    return QColor(0xd6, 0x40, 0x50);
}

QColor SpellHighlighter::grammarUnderlineColor()
{
    return QColor(0x00, 0xcc, 0x66);
}

SpellHighlighter::SpellHighlighter(QTextDocument *document, QObject *parent)
    : QSyntaxHighlighter(document)
    , m_lintTimer(new QTimer(this))
{
    if (parent)
        setParent(parent);
    m_lintTimer->setSingleShot(true);
    m_lintTimer->setInterval(400);
    connect(m_lintTimer, &QTimer::timeout, this, &SpellHighlighter::runGrammarLint);
    // Schedule on contentsChange (not contentsChanged): rehighlight() and
    // other format-only work emit contentsChanged but no contentsChange, so
    // this prevents the lint from re-arming itself into an infinite loop.
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
    m_lintThread->setObjectName(QStringLiteral("harper-lint"));
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

void SpellHighlighter::refresh()
{
    m_grammarIssues.clear();
    rehighlight();
    if (m_grammar && m_grammarEnabled)
        m_lintTimer->start();
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
    static const QRegularExpression fenceRe(R"(^\s*(?:```+|~~~+))");
    static const QRegularExpression frontMatterRe(R"(^\s*---\s*$)");

    const int blockNumber = currentBlock().blockNumber();
    const int state = previousBlockState(); // 0 = plain, 1 = fenced code, 2 = front matter
    const bool opener = fenceRe.match(text).hasMatch();
    const bool isFrontMatterBoundary = frontMatterRe.match(text).hasMatch();

    m_spellHits.remove(blockNumber);

    int newState = 0;
    if (state == 1) {
        newState = opener ? 0 : 1;
    } else if (state == 2) {
        newState = isFrontMatterBoundary ? 0 : 2;
    } else if (blockNumber == 0 && isFrontMatterBoundary) {
        newState = 2;
    } else if (opener) {
        newState = 1;
    }
    setCurrentBlockState(newState);

    const bool checkable = state != 1 && state != 2 && !opener
        && !(blockNumber == 0 && isFrontMatterBoundary);
    if (!checkable) {
        setFormat(0, text.length(), QTextCharFormat());
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
        for (const WordHit &word : scanWords(text)) {
            if (!m_checker->checkWord(word.text)) {
                m_spellHits[blockNumber].append({word.start, word.length});
                applied = true;
            }
        }
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
