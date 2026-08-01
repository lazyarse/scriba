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
    int headerAtPos(int y) const;

    static qreal firstLineTextCenterY(const QTextBlock &block);

signals:
    void foldToggled(int blockNumber);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;

public:
    void updateWidth();

private:
    int preferredWidth() const;

    Editor *m_editor;
    bool m_showLineNumbers = true;
    QSet<int> m_foldableBlocks;
    QSet<int> m_foldedBlocks;
};
