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
    void setFoldIconsVisible(bool visible);
    bool lineNumbersVisible() const { return m_showLineNumbers; }
    bool foldIconsVisible() const { return m_showFoldIcons; }

    void setFoldableBlocks(const QSet<int> &foldable);
    void setFoldedBlocks(const QSet<int> &folded);
    int headerAtPos(int y) const;

signals:
    void foldToggled(int blockNumber);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

public:
    void updateWidth();

private:
    int preferredWidth() const;

    Editor *m_editor;
    bool m_showLineNumbers = true;
    bool m_showFoldIcons = true;
    QSet<int> m_foldableBlocks;
    QSet<int> m_foldedBlocks;
};
