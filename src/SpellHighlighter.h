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

#include "GrammarChecker.h"
#include <QHash>
#include <QList>
#include <QString>
#include <QSyntaxHighlighter>
#include <QThread>
#include <QVector>

class SpellChecker;
class GrammarChecker;
class QTimer;
class QColor;

// Runs the (expensive, whole-document) grammar check on a background thread.
// The result is delivered back as a queued signal with a generation tag so
// stale results (superseded by newer edits) can be dropped.
class GrammarLintWorker : public QObject
{
    Q_OBJECT

public:
    explicit GrammarLintWorker(GrammarChecker *checker);

public slots:
    void doLint(quint64 generation, const QString &text);

signals:
    void lintFinished(quint64 generation, const QString &text,
                      const QList<GrammarChecker::Issue> &issues);

private:
    GrammarChecker *m_checker = nullptr;
};

// Applies red spell-check underlines and green grammar wave underlines to the
// editor's QTextDocument. Markdown syntax that must not be checked (fenced
// code, inline code, URLs, HTML tags, emoji shortcodes, math, front matter)
// is skipped via per-block state tracking and a shared word scanner.
//
// Spelling is checked word-by-word inside highlightBlock() (hunspell is fast
// enough for that). Grammar checking is whole-document and expensive, so it
// runs on a debounced timer and the actual check happens on a background
// thread; per-block issue ranges are cached here.
class SpellHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    struct WordHit {
        QString text;
        int start = 0; // offset within the line
        int length = 0;
    };

    struct GrammarHit {
        int start = 0; // offset within the block
        int length = 0;
        QString message;
        QVector<GrammarChecker::Issue::Suggestion> suggestions;
    };

    // Squiggle colors, shared with Editor's custom underline painting.
    static QColor spellUnderlineColor();
    static QColor grammarUnderlineColor();

    explicit SpellHighlighter(QTextDocument *document, QObject *parent = nullptr);
    ~SpellHighlighter() override;

    void setChecker(SpellChecker *checker);
    void setGrammarChecker(GrammarChecker *checker);
    void setSpellCheckingEnabled(bool enabled);
    void setGrammarCheckingEnabled(bool enabled);

    // Re-runs the pending grammar lint and re-applies underlines. Call after
    // dictionary changes (add/ignore word, language switch).
    void refresh();

    // Markdown-aware word scan: returns the words of `line` that are eligible
    // for spell checking (outside code, URLs, tags, emoji, etc.).
    static QList<WordHit> scanWords(const QString &line);

    // Grammar issues (start offset + length within the block) for a block.
    QVector<GrammarHit> grammarIssuesInBlock(int blockNumber) const;

    // Misspelled word ranges (start offset + length within the block) for a
    // block. The Editor paints thick underlines over these + grammar issues.
    QVector<GrammarHit> spellHitsInBlock(int blockNumber) const;

protected:
    void highlightBlock(const QString &text) override;

private:
    void scheduleGrammarLint(int charsRemoved, int charsAdded);
    void runGrammarLint();
    void onLintFinished(quint64 generation, const QString &text,
                        const QList<GrammarChecker::Issue> &issues);
    void ensureLintWorker();
    static QVector<QPair<int, int>> protectedRanges(const QString &line);

    SpellChecker *m_checker = nullptr;
    GrammarChecker *m_grammar = nullptr;
    QTimer *m_lintTimer = nullptr;
    QThread *m_lintThread = nullptr;
    GrammarLintWorker *m_lintWorker = nullptr;
    quint64 m_lintGeneration = 0;
    bool m_spellEnabled = true;
    bool m_grammarEnabled = false;
    // blockNumber → grammar issues within the block
    QHash<int, QVector<GrammarHit>> m_grammarIssues;
    // blockNumber → misspelled word ranges within the block
    QHash<int, QVector<GrammarHit>> m_spellHits;
};
