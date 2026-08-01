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
#include "Gutter.h"
#include "Editor.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QSettings>
#include <QEvent>
#include <QCoreApplication>
#include <QAbstractTextDocumentLayout>
#include "Preferences.h"

Gutter::Gutter(Editor *editor)
    : QWidget(editor)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("gutter"));
    setCursor(Qt::ArrowCursor);
    updateWidth();
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { update(); });
}

void Gutter::setLineNumbersVisible(bool visible)
{
    m_showLineNumbers = visible;
    updateWidth();
    update();
}

void Gutter::setFoldableBlocks(const QSet<int> &foldable)
{
    m_foldableBlocks = foldable;
    update();
}

void Gutter::setFoldedBlocks(const QSet<int> &folded)
{
    m_foldedBlocks = folded;
    update();
}

void Gutter::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        update();
}

qreal Gutter::firstLineTextCenterY(const QTextBlock &block)
{
    const QTextLayout *layout = block.layout();
    if (!layout || layout->lineCount() == 0)
        return 0.0;
    const QTextLine line0 = layout->lineAt(0);
    return layout->position().y() + line0.y() + (line0.ascent() + line0.descent()) / 2.0;
}

int Gutter::headerAtPos(int y) const
{
    QTextCursor cursor0(m_editor->document());
    cursor0.setPosition(0);
    int docTopY = m_editor->viewport()->mapTo(m_editor, m_editor->cursorRect(cursor0).topLeft()).y()
                  - (int)m_editor->document()->documentMargin();

    QTextBlock block = m_editor->document()->firstBlock();
    while (block.isValid()) {
        QTextBlock next = block.next();
        if (!block.isVisible()) {
            block = next;
            continue;
        }
        QRectF r = m_editor->document()->documentLayout()->blockBoundingRect(block);
        int y0 = docTopY + (int)r.y();
        int blockH = (int)r.height();
        if (y >= y0 && y < y0 + blockH) {
            if (m_foldableBlocks.contains(block.blockNumber()))
                return block.blockNumber();
            return -1;
        }
        block = next;
    }
    return -1;
}

int Gutter::preferredWidth() const
{
    int w = 0;
    if (m_showLineNumbers) {
        int digits = 1;
        int lines = m_editor->document()->blockCount();
        if (lines >= 1000) digits = 4;
        else if (lines >= 100) digits = 3;
        else if (lines >= 10) digits = 2;
        w += fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 16;
    }
    w += 14;
    return w;
}

void Gutter::updateWidth()
{
    setFixedWidth(preferredWidth());
}

void Gutter::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QSettings s;
    bool colorOverride = s.value(Preferences::GutterColorOverride, false).toBool();
    QColor bg;
    QColor textColor;
    if (colorOverride) {
        bg = QColor(s.value(Preferences::GutterBgColor, "#f0f0f0").toString());
        textColor = QColor(s.value(Preferences::GutterTextColor, "#888888").toString());
    } else {
        bg = palette().window().color();
        textColor = palette().windowText().color();
    }
    painter.fillRect(rect(), bg);

    QPen linePen(textColor);
    QPen foldPen(textColor.darker(120));
    QFont lineFont = m_editor->document()->defaultFont();
    lineFont.setPointSize(qMax(8, lineFont.pointSize() - 1));
    painter.setFont(lineFont);

    QTextCursor cursor0(m_editor->document());
    cursor0.setPosition(0);
    int docTopY = m_editor->viewport()->mapTo(m_editor, m_editor->cursorRect(cursor0).topLeft()).y()
                  - (int)m_editor->document()->documentMargin();
    int iconW = 12;
    int iconH = 12;

    QTextBlock block = m_editor->document()->firstBlock();
    while (block.isValid()) {
        QTextBlock next = block.next();

        if (!block.isVisible()) {
            block = next;
            continue;
        }

        QRectF r = m_editor->document()->documentLayout()->blockBoundingRect(block);
        int y0 = docTopY + (int)r.y();
        int lineH = (int)r.height();

        qreal textCenterY;
        const QTextLayout *layout = block.layout();
        if (layout && layout->lineCount() > 0)
            textCenterY = docTopY + Gutter::firstLineTextCenterY(block);
        else
            textCenterY = y0 + lineH / 2.0;

        int blockNum = block.blockNumber();
        int paintX = 0;

        if (m_showLineNumbers) {
            painter.setPen(linePen);
            QString num = QString::number(blockNum + 1);
            int numW = painter.fontMetrics().horizontalAdvance(num);
            int x = width() - iconW - 4 - numW;
            int numH = painter.fontMetrics().height();
            int numY = qRound(textCenterY - numH / 2.0);
            painter.drawText(x, numY, numW, numH, Qt::AlignRight | Qt::AlignVCenter, num);
        }

        if (m_foldableBlocks.contains(blockNum)) {
            painter.setPen(foldPen);
            painter.setBrush(foldPen.color());
            bool folded = m_foldedBlocks.contains(blockNum);

            int cx = m_showLineNumbers ? width() - iconW - 2 : (width() - iconW) / 2;
            int cy = qRound(textCenterY - iconH / 2.0);
            QRect iconRect(cx, cy, iconW, iconH);

            if (folded) {
                const QPointF triangle[3] = {
                    QPointF(iconRect.left() + 2, iconRect.top() + 1),
                    QPointF(iconRect.left() + 2, iconRect.bottom() - 1),
                    QPointF(iconRect.right() - 1, iconRect.center().y())
                };
                painter.drawConvexPolygon(triangle, 3);
            } else {
                const QPointF triangle[3] = {
                    QPointF(iconRect.left() + 1, iconRect.top() + 2),
                    QPointF(iconRect.right() - 1, iconRect.top() + 2),
                    QPointF(iconRect.center().x(), iconRect.bottom() - 1)
                };
                painter.drawConvexPolygon(triangle, 3);
            }
        }

        block = next;
    }

    painter.setPen(textColor);
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

void Gutter::mousePressEvent(QMouseEvent *event)
{
    int blockNum = headerAtPos((int)event->position().y());
    if (blockNum >= 0)
        emit foldToggled(blockNum);
}

void Gutter::wheelEvent(QWheelEvent *event)
{
    QCoreApplication::sendEvent(m_editor->verticalScrollBar(), event);
}
