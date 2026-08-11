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

#include <gtest/gtest.h>
#include <QString>
#include <Qt>
#include <type_traits>

class Editor;

struct CompletionPrefs
{
    bool file = false;
    bool emoji = false;
    bool language = false;

    static CompletionPrefs all() { return {true, true, true}; }
};

class EditorTestHarness : public testing::Test
{
public:
    explicit EditorTestHarness(CompletionPrefs prefs = {});

protected:
    void SetUp() override;
    void TearDown() override;

    void typeText(const QString &text);
    void press(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier);
    void enter();
    void typeLine(const QString &text);
    // Sets `text` on the clipboard and pastes it through the editor's real
    // QTextEdit::paste() -> insertFromMimeData path.
    void pasteText(const QString &text);

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
    CompletionPrefs m_prefs;

    template <typename T>
    void typeOrPress(const T &item)
    {
        if constexpr (std::is_convertible_v<T, QString>)
            typeText(item);
        else
            press(item);
    }
};
