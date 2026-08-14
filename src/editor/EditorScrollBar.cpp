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
#include "EditorScrollBar.h"

#include "prefs/Preferences.h"
#include "spell/SpellHighlighter.h"

#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QSettings>
#include <QTextBlock>
#include <QTextDocument>

EditorScrollBar::EditorScrollBar(QWidget *parent)
    : QScrollBar(Qt::Vertical, parent)
{
}

void EditorScrollBar::setHighlighter(SpellHighlighter *highlighter)
{
    m_highlighter = highlighter;
    invalidate();
}

void EditorScrollBar::applySettings()
{
    m_enabled = QSettings().value(Preferences::ErrorScrollbarEnabled, true).toBool();
    update();
}

void EditorScrollBar::invalidate()
{
    m_dirty = true;
    update();
}

qreal EditorScrollBar::documentFraction(qreal blockTop, qreal docHeight)
{
    if (docHeight <= 0.0)
        return 0.0;
    return qBound(0.0, blockTop / docHeight, 1.0);
}

void EditorScrollBar::rebuildIndex()
{
    m_entries.clear();
    if (!m_highlighter)
        return;
    const QTextDocument *doc = m_highlighter->document();
    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        if (!block.isVisible())
            continue;
        const int n = block.blockNumber();
        quint8 flags = 0;
        if (!m_highlighter->spellHitsInBlock(n).isEmpty())
            flags |= Flag::Spell;
        if (!m_highlighter->grammarIssuesInBlock(n).isEmpty())
            flags |= Flag::Grammar;
        if (!m_highlighter->linkIssuesInBlock(n).isEmpty())
            flags |= Flag::Link;
        if (!m_highlighter->markdownHitsInBlock(n).isEmpty())
            flags |= Flag::Markdown;
        if (flags)
            m_entries.append({n, flags});
    }
}

void EditorScrollBar::paintEvent(QPaintEvent *event)
{
    // Themed track + handle come from the app-wide QSS.
    QScrollBar::paintEvent(event);

    if (!m_enabled || !m_highlighter || height() <= 0)
        return;
    if (m_dirty) {
        rebuildIndex();
        m_dirty = false;
    }
    if (m_entries.isEmpty())
        return;

    const QTextDocument *doc = m_highlighter->document();
    const qreal docHeight = qMax<qreal>(1.0,
        doc->documentLayout()->documentSize().height());
    const int trackLen = height();

    const struct { Flag flag; QColor color; } types[4] = {
        {Flag::Spell,    SpellHighlighter::spellUnderlineColor()},
        {Flag::Grammar,  SpellHighlighter::grammarUnderlineColor()},
        {Flag::Link,     SpellHighlighter::linkUnderlineColor()},
        {Flag::Markdown, SpellHighlighter::markdownUnderlineColor()},
    };

    QPainter p(this);
    p.setClipRect(event->rect());
    p.setRenderHint(QPainter::Antialiasing, false);
    for (const Entry &e : m_entries) {
        const QTextBlock block = doc->findBlockByNumber(e.blockNumber);
        if (!block.isValid() || !block.isVisible())
            continue;
        const qreal center = doc->documentLayout()->blockBoundingRect(block).center().y();
        int y = qRound(documentFraction(center, docHeight) * trackLen);
        int offset = 0; // stack each present type 2 px below the previous one
        for (const auto &t : types) {
            if (!(e.flags & t.flag))
                continue;
            const int lineY = y + offset;
            p.fillRect(1, lineY, width() - 2, 2, t.color);
            offset += 2;
        }
    }
}