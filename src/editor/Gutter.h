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

#include <QWidget>
#include <QSet>
#include <QColor>
#include <QPixmap>

class Editor;
class QTextBlock;

class Gutter : public QWidget
{
    Q_OBJECT

public:
    explicit Gutter(Editor *editor);

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const { return m_showLineNumbers; }

    void setFoldableBlocks(const QSet<int> &foldable);
    void setFoldedBlocks(const QSet<int> &folded);
    void setChartBlocks(const QSet<int> &chartBlocks);
    int headerAtPos(int y) const;
    // The chart-fence opening block whose first content line (opening fence
    // + 1) is at gutter y, or -1. The pencil icon for a chart sits on that
    // content line, just inside the block, so it never collides with the
    // fold triangle on the opening-fence row.
    int chartFenceAtPos(int y) const;

    static qreal firstLineTextCenterY(const QTextBlock &block);

signals:
    void foldToggled(int blockNumber);
    void chartEditRequested(int blockNumber);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;

public:
    void updateWidth();

private:
    int preferredWidth() const;
    QPixmap pencilPixmap(const QColor &color, int size);

    Editor *m_editor;
    bool m_showLineNumbers = true;
    QSet<int> m_foldableBlocks;
    QSet<int> m_foldedBlocks;
    QSet<int> m_chartBlocks;
    QColor m_pencilColor;
    QPixmap m_pencilPixmap;
};
