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
#include "validation/MarkdownChecker.h"
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QString>
#include <QSyntaxHighlighter>
#include <QVector>
#include <memory>

class SpellChecker;
class GrammarChecker;
class QTimer;
class QColor;

// Runs the (expensive, whole-document) grammar check on a background thread.
// The result is delivered back as a queued signal with a generation tag so
// stale results (superseded by newer edits) can be dropped.
//
// The worker is a process-wide singleton shared by every editor: requests are
// dispatched with QMetaObject::invokeMethod(worker, lambda, QueuedConnection)
// and carry their own std::shared_ptr<GrammarChecker>, so destroying an editor
// (or its SpellHighlighter) never has to join a thread or wait for a check in
// flight — the worker thread keeps running and the checker stays alive via the
// request's shared ownership. This is what keeps closing a tab (even one with
// a large document and grammar checking on) instantaneous.
class GrammarLintWorker : public QObject
{
    Q_OBJECT

public:
    explicit GrammarLintWorker() = default;

public slots:
    void doLint(quint64 generation, std::shared_ptr<GrammarChecker> checker,
                const QString &text);

signals:
    void lintFinished(quint64 generation, const QString &text,
                      const QList<GrammarChecker::Issue> &issues);
};

// Applies red spell-check underlines, green grammar wave underlines and amber
// broken-link underlines to the editor's QTextDocument. Markdown syntax that
// must not be checked (fenced code, inline code, URLs, HTML tags, emoji
// shortcodes, math, front matter) is skipped via per-block state tracking and
// a shared word scanner.
//
// Spelling is checked word-by-word (hunspell is fast enough for that), but
// not on every keystroke: an edit makes its blocks "stale" (underlines
// cleared) until the check runs again, which happens immediately when the
// edit inserts a separator (space, punctuation, newline) — i.e. the word is
// complete — or after a short debounce, so words that are still being typed
// are not flagged as typos. Grammar checking is whole-document and
// expensive, so it runs on a debounced timer and the actual check happens
// on a background thread; per-block issue ranges are cached here.
//
// Broken-link checking (file targets that do not exist, malformed http(s)
// URLs, reference usages without a definition) is cheap — a regex match plus
// a filesystem stat per target — so it rides along with the spell-check
// passes and recomputes every block wholesale (a `[ref]:` definition added on
// one line must re-flag usages on other lines). Partially typed targets like
// `](pa` never match the closing-paren regex, so nothing is flagged while the
// path is being typed; the closing `)` is a word separator and triggers an
// immediate check.
class SpellHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    struct WordHit {
        QString text;
        int start = 0; // offset within the line
        int length = 0;
    };

    // One misspelled word in the document, as found by scanDocument().
    struct SpellIssue {
        int blockNumber = 0;
        int start = 0; // offset within the block
        int length = 0;
        QString word;
    };

    struct GrammarHit {
        int start = 0; // offset within the block
        int length = 0;
        QString message;
        QVector<GrammarChecker::Issue::Suggestion> suggestions;
    };

    // One broken-link hit in a whole document, as found by scanLinkIssues().
    // line and col are 1-based within the document.
    struct LinkHit {
        int line = 0;
        int col = 0;
        int length = 0;
        QString message;
    };

    // Squiggle colors, shared with Editor's custom underline painting.
    static QColor spellUnderlineColor();
    static QColor grammarUnderlineColor();
    static QColor linkUnderlineColor();
    static QColor markdownUnderlineColor();
    // Re-reads the underline colors from settings (called after a preference
    // change; also resets the cache in tests).
    static void reloadUnderlineColors();
    // Background highlight used by the Check Spelling dialog to point at the
    // current error.
    static QColor spellHighlightColor();

    explicit SpellHighlighter(QTextDocument *document, QObject *parent = nullptr);
    ~SpellHighlighter() override;

    void setChecker(SpellChecker *checker);
    void setGrammarChecker(std::shared_ptr<GrammarChecker> checker);
    void setSpellCheckingEnabled(bool enabled);
    bool spellCheckingEnabled() const { return m_spellEnabled; }
    void setGrammarCheckingEnabled(bool enabled);
    void setLinkCheckingEnabled(bool enabled);
    // Markdown-consistency underlines (heading-level skips, duplicate
    // headings, `#` headings without a space, ...). Off by default: it is a
    // per-keystroke distraction, so like grammar it must be opted into.
    void setMarkdownCheckingEnabled(bool enabled);
    // The subset of markdown-consistency checks to underline. The checks are
    // read from the preferences; an empty set underlines nothing. Changing
    // them re-runs the scan so underlines follow immediately.
    void setMarkdownChecks(const QSet<MarkdownChecker::Check> &checks);
    // The document's file path: link targets are resolved relative to its
    // directory (an empty path resolves against the current working
    // directory). Triggers a re-check so underlines follow the new base.
    void setCurrentFile(const QString &path);
    // Base dir for relative link targets when the document has no file (an
    // untitled tab). Used only when m_currentFile is empty; the CWD fallback
    // remains for the no-corpus case.
    void setFallbackLinkBaseDir(const QString &dir);
    // Runs checks synchronously to completion even on large documents (the
    // normal large-document behaviour defers spell scanning into chunks and
    // skips the grammar lint). Used by scanLinkIssues()/validation, which must
    // read the finished caches immediately after setting the file.
    void setForceSyncChecks(bool force);

    // Re-runs the pending grammar lint and re-applies underlines. Call after
    // dictionary changes (add/ignore word, language switch).
    void refresh();

    // Markdown-aware word scan: returns the words of `line` that are eligible
    // for spell checking (outside code, URLs, tags, emoji, etc.).
    static QList<WordHit> scanWords(const QString &line);

    // Whole-document misspelled-word scan for the Check Spelling dialog: walks
    // every block with the same fence/front-matter state machine and word
    // scanner the underlines use, so the dialog reports exactly what the
    // editor flags. Returns issues in document order.
    static QVector<SpellIssue> scanDocument(QTextDocument *document,
                                            SpellChecker *checker);

    // Whole-document broken-link scan (file targets, URLs, reference usages,
    // heading anchors) using the same pass the underlines use. `baseDir`
    // resolves relative targets; empty uses the current working directory.
    // Returns hits with 1-based line/col. Used by the Validation Report.
    static QVector<LinkHit> scanLinkIssues(const QString &text,
                                           const QString &baseDir);

    // Grammar issues (start offset + length within the block) for a block.
    QVector<GrammarHit> grammarIssuesInBlock(int blockNumber) const;

    // Misspelled word ranges (start offset + length within the block) for a
    // block. The Editor paints thick underlines over these + grammar issues.
    QVector<GrammarHit> spellHitsInBlock(int blockNumber) const;

    // Broken-link ranges (start offset + length within the block) for a block.
    QVector<GrammarHit> linkIssuesInBlock(int blockNumber) const;

    // Markdown-consistency ranges (start offset + length within the block) for
    // a block. Empty for blocks that are not part of a finding.
    QVector<GrammarHit> markdownHitsInBlock(int blockNumber) const;

signals:
    // The spell/link hit caches were refreshed (after a debounced or
    // word-boundary check). The Editor repaints its underline overlay on this.
    // Emitted instead of calling rehighlightBlock(): that would run inside the
    // document's contentsChange emission and re-enter the highlighter.
    void spellHitsChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void scheduleSpellCheck(int position, int charsRemoved, int charsAdded);
    void runSpellCheck();
    void continueSpellScan();
    void scheduleGrammarLint(int charsRemoved, int charsAdded);
    void runGrammarLint();
    void onLintFinished(quint64 generation, const QString &text,
                        const QList<GrammarChecker::Issue> &issues);
    static QVector<QPair<int, int>> protectedRanges(const QString &line);

    // Documents at or above kLargeDocBlocks (StaticHelpers.h) skip the eager
    // per-block spell check inside highlightBlock() and the whole-document
    // grammar lint, and run their spell scan in ~kSpellScanChunkBlocks chunks
    // on a 0 ms timer so the UI thread stays responsive while a large file is
    // being opened.
    static constexpr int kSpellScanChunkBlocks = 500;
    bool largeDocument() const;

    // Everything one whole-document pass collects for the link scan: the
    // `[name]:` reference definitions and the heading slugs the preview would
    // assign (both respect fenced code / front matter state).
    struct DocumentContext {
        QSet<QString> refDefs;
        QSet<QString> headingSlugs;
    };
    DocumentContext collectDocumentContext() const;
    // Heading slugs for raw lines of another markdown file (used to validate
    // cross-document `file#anchor` links, via a small mtime/size cache).
    QSet<QString> headingSlugsFromLines(const QStringList &lines) const;
    QSet<QString> crossDocSlugs(const QString &absolutePath);
    // Broken-link hits (file targets, URLs, reference usages, anchors) in one
    // line. `currentSlugs` are the headings of the document being edited.
    QVector<GrammarHit> scanLinkHits(const QString &line,
                                     const QSet<QString> &refDefs,
                                     const QSet<QString> &currentSlugs);

    SpellChecker *m_checker = nullptr;
    std::shared_ptr<GrammarChecker> m_grammar;
    QTimer *m_lintTimer = nullptr;
    QTimer *m_spellTimer = nullptr;
    // Drives the chunked large-document spell scan: single-shot 0 ms timer,
    // re-armed by continueSpellScan() until the document is exhausted. Using a
    // member timer (not singleShot()) guarantees at most one continuation is
    // pending, so a scan restarted mid-flight can never double-walk the
    // document.
    QTimer *m_chunkTimer = nullptr;
    // The shared process-wide lint worker (see GrammarLintWorker). Each
    // highlighter keeps its own connection to its onLintFinished() slot.
    QMetaObject::Connection m_grammarLintConnection;
    quint64 m_lintGeneration = 0;
    bool m_spellEnabled = true;
    bool m_grammarEnabled = false;
    bool m_linkEnabled = true;
    bool m_markdownEnabled = false;
    bool m_forceSyncChecks = false;
    // The markdown-consistency checks to underline; empty disables them all.
    QSet<MarkdownChecker::Check> m_markdownChecks = MarkdownChecker::defaultChecks();
    // The document's file path, for resolving relative link targets.
    QString m_currentFile;
    // Base dir for relative link targets in untitled tabs (corpus root).
    QString m_fallbackLinkBaseDir;
    // Cached heading indexes of other documents, keyed by absolute path and
    // invalidated when the file's mtime/size changes.
    struct AnchorCache {
        QDateTime mtime;
        qint64 size = 0;
        QSet<QString> slugs;
    };
    QHash<QString, AnchorCache> m_anchorCache;
    // Blocks edited since their last spell check: underlines stay cleared
    // until runSpellCheck() re-checks them.
    QSet<int> m_staleBlocks;
    // Document block count from the previous edit, to detect block
    // insertions/removals (they renumber every following block, so all of
    // them must be re-checked, not just the edited range).
    int m_lastBlockCount = 0;
    // Chunked-scan continuation state (see kLargeDocBlocks). m_scanContext is
    // collected once at the start of a pass for the link half; the block walk
    // itself resumes from m_scanBlockNumber with the fence/front-matter state
    // carried in m_scanState, accumulating m_scanAny until the document is
    // exhausted (when spellHitsChanged() is emitted once).
    DocumentContext m_scanContext;
    int m_scanBlockNumber = 0;
    int m_scanState = 0;
    bool m_scanAny = false;
    bool m_scanChunked = false;
    // blockNumber → grammar issues within the block
    QHash<int, QVector<GrammarHit>> m_grammarIssues;
    // blockNumber → misspelled word ranges within the block
    QHash<int, QVector<GrammarHit>> m_spellHits;
    // blockNumber → broken-link ranges within the block
    QHash<int, QVector<GrammarHit>> m_linkHits;
    // blockNumber → markdown-consistency ranges within the block
    QHash<int, QVector<GrammarHit>> m_markdownHits;
};
