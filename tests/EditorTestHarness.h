#pragma once

#include <gtest/gtest.h>
#include <QString>
#include <Qt>
#include <type_traits>

class Editor;

class EditorTestHarness : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    void typeText(const QString &text);
    void press(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier);
    void enter();
    void typeLine(const QString &text);

    template <typename... Items>
    void run(Items... items)
    {
        (typeOrPress(items), ...);
    }

    void setContent(const QString &content);
    void waitForFolds();
    void placeCursor(int block, int column);
    void placeCursorAtEnd();
    void selectLines(int firstBlock, int lastBlock);

    QString text() const;
    int cursorBlock() const;
    int cursorColumn() const;
    void assertCursor(int block, int column) const;

    Editor *editor = nullptr;

private:
    template <typename T>
    void typeOrPress(const T &item)
    {
        if constexpr (std::is_convertible_v<T, QString>)
            typeText(item);
        else
            press(item);
    }
};
