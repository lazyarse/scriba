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
#include <QFontMetricsF>
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

void Gutter::setFoldIconsVisible(bool visible)
{
    m_showFoldIcons = visible;
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

int Gutter::headerAtPos(int y) const
{
    QTextBlock block = m_editor->document()->firstBlock();
    int viewY = -m_editor->verticalScrollBar()->value();
    while (block.isValid()) {
        QTextBlock next = block.next();
        if (!block.isVisible())
            block = next;
        int blockY = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).y();
        int blockH = (int)m_editor->document()->documentLayout()->blockBoundingRect(block).height();
        int y0 = viewY + blockY;
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
    if (m_showFoldIcons)
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
    QFont lineFont = painter.font();
    lineFont.setPointSize(qMax(8, lineFont.pointSize() - 1));
    painter.setFont(lineFont);

    int viewY = -m_editor->verticalScrollBar()->value();
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
        int y0 = viewY + (int)r.y();
        int lineH = (int)r.height();

        int blockNum = block.blockNumber();
        int paintX = 0;

        if (m_showLineNumbers) {
            painter.setPen(linePen);
            QString num = QString::number(blockNum + 1);
            int numW = fontMetrics().horizontalAdvance(num);
            int x = width() - (m_showFoldIcons ? iconW + 4 : 4) - numW;
            painter.drawText(x, y0, numW, lineH, Qt::AlignRight | Qt::AlignVCenter, num);
        }

        if (m_showFoldIcons && m_foldableBlocks.contains(blockNum)) {
            painter.setPen(foldPen);
            painter.setBrush(foldPen.color());
            bool folded = m_foldedBlocks.contains(blockNum);

            int cx = m_showLineNumbers ? width() - iconW - 2 : (width() - iconW) / 2;
            int cy = y0 + (lineH - iconH) / 2;
            QRect iconRect(cx, cy, iconW, iconH);

            if (folded) {
                static const QPointF triangle[3] = {
                    QPointF(iconRect.left() + 2, iconRect.top() + 1),
                    QPointF(iconRect.left() + 2, iconRect.bottom() - 1),
                    QPointF(iconRect.right() - 1, iconRect.center().y())
                };
                painter.drawConvexPolygon(triangle, 3);
            } else {
                static const QPointF triangle[3] = {
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
