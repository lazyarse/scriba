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

#include <QScrollBar>
#include <QVector>

class SpellHighlighter;

// The editor's vertical scrollbar: a theme-styled QScrollBar (the app-wide QSS
// paints the track/handle) that additionally paints one thin horizontal line
// per error type present on each block that has a spelling, grammar,
// broken-link or markdown-consistency hit. Lines use the exact colors of the
// editor's underlines (SpellHighlighter::*UnderlineColor) and sit at the
// block's position in the document, compressed into the full track height — a
// whole-document overview. Bands that would land under the thumb are skipped
// so the handle stays readable. One line per type per block, stacked 2 px
// apart when a block has several types.
class EditorScrollBar : public QScrollBar
{
    Q_OBJECT

public:
    // Error type bits, mirroring the four underline passes.
    enum Flag : quint8 {
        Spell    = 1 << 0,
        Grammar  = 1 << 1,
        Link     = 1 << 2,
        Markdown = 1 << 3,
    };

    // One flagged block in the document.
    struct Entry {
        int blockNumber = 0;
        quint8 flags = 0;
    };

    explicit EditorScrollBar(QWidget *parent = nullptr);

    // The SpellHighlighter whose per-block hit caches feed the markers. Its
    // document() also provides the block→pixel mapping.
    void setHighlighter(SpellHighlighter *highlighter);
    SpellHighlighter *highlighter() const { return m_highlighter; }

    // Re-reads the ErrorScrollbarEnabled preference and repaints. Wired by the
    // Editor into applySpellSettings()/refreshUnderlines() so preference and
    // underline-color changes apply immediately.
    void applySettings();

    // Marks the block→flags index stale and repaints. Called when the hit
    // caches refresh (spellHitsChanged) or the document changes; the index is
    // rebuilt lazily on the next paint.
    void invalidate();

    // The current index: one Entry per visible block that has at least one
    // flagged type. Exposed for tests (a grab()/paint flushes a rebuild).
    QVector<Entry> entries() const { return m_entries; }

    // Fraction of the document height at which a block sitting at `blockTop`
    // (device-independent px) belongs, clamped to [0, 1]. Pure/static so it is
    // unit-testable.
    static qreal documentFraction(qreal blockTop, qreal docHeight);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildIndex();

    SpellHighlighter *m_highlighter = nullptr;
    QVector<Entry> m_entries;
    bool m_dirty = true;
    bool m_enabled = true;
};