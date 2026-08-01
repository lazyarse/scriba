#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QSyntaxHighlighter>
#include <QVector>

class SpellChecker;
class GrammarChecker;
class QTimer;
class QColor;

// Applies red spell-check underlines and green grammar wave underlines to the
// editor's QTextDocument. Markdown syntax that must not be checked (fenced
// code, inline code, URLs, HTML tags, emoji shortcodes, math, front matter)
// is skipped via per-block state tracking and a shared word scanner.
//
// Spelling is checked word-by-word inside highlightBlock() (hunspell is fast
// enough for that). Grammar checking is whole-document and expensive, so it
// runs on a debounced timer and caches per-block issue ranges.
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
    };

    // Squiggle colors, shared with Editor's custom underline painting.
    static QColor spellUnderlineColor();
    static QColor grammarUnderlineColor();

    explicit SpellHighlighter(QTextDocument *document, QObject *parent = nullptr);

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
    void scheduleGrammarLint();
    void runGrammarLint();
    static QVector<QPair<int, int>> protectedRanges(const QString &line);

    SpellChecker *m_checker = nullptr;
    GrammarChecker *m_grammar = nullptr;
    QTimer *m_lintTimer = nullptr;
    bool m_spellEnabled = true;
    bool m_grammarEnabled = false;
    // blockNumber → grammar issues within the block
    QHash<int, QVector<GrammarHit>> m_grammarIssues;
    // blockNumber → misspelled word ranges within the block
    QHash<int, QVector<GrammarHit>> m_spellHits;
};
