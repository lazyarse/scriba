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
