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
#include <gtest/gtest.h>

#include <QApplication>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "mainwindow/MainWindow.h"
#include "editor/Editor.h"
#include "prefs/Preferences.h"
#include "corpus/Corpus.h"
#include "TestConfig.h"

namespace {

// A multi-block document tall enough that an 800px-tall editor has plenty of
// vertical scroll range (the restore sets an absolute scrollbar value).
QString tallDocument()
{
    QStringList lines;
    for (int i = 0; i < 300; ++i)
        lines << QStringLiteral("Line %1").arg(i);
    return lines.join('\n');
}

class CorpusRestoreTest : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_corpus_restore";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override
    {
        QSettings s;
        s.clear();
        s.setValue(Preferences::ReopenLastCorpus, false);
        s.setValue(Preferences::AutoSaveOnExit, false);
        s.setValue(Preferences::AutoSaveInterval, 0);
        s.setValue(Preferences::RestorePositions, true);   // default: on

        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_corpusPath = m_dir->path() + "/corpus.scriba";
    }

    void TearDown() override
    {
        delete m_window;
        m_window = nullptr;
    }

    // Writes a .scriba holding one embedded document with a known cursor +
    // scroll state, then opens it in a fresh MainWindow.
    void writeAndOpen(int block, int col, int scroll)
    {
        Corpus c;
        c.filePath = m_corpusPath;
        c.name = QStringLiteral("RestoreTest");
        c.active = 0;
        c.documents = {
            CorpusDocument{ .path = QString(),
                            .content = tallDocument(),
                            .cursorBlock = block,
                            .cursorCol = col,
                            .scroll = scroll },
        };
        ASSERT_TRUE(c.save());

        m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
        m_window->resize(1200, 800);
        m_window->show();
        QApplication::processEvents();
        m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
        QApplication::processEvents();
        QTest::qWait(200);      // let the deferred scroll-restore singleShot run
    }

    QString m_corpusPath;
    std::unique_ptr<QTemporaryDir> m_dir;
    MainWindow *m_window = nullptr;
};

TEST_F(CorpusRestoreTest, RestorePositionsOnAppliesSavedCursorAndScroll)
{
    writeAndOpen(/*block=*/10, /*col=*/3, /*scroll=*/120);

    Editor *ed = m_window->editor();
    ASSERT_NE(ed, nullptr);
    EXPECT_EQ(ed->textCursor().blockNumber(), 10);
    EXPECT_EQ(ed->textCursor().positionInBlock(), 3);
    EXPECT_EQ(ed->verticalScrollBar()->value(), 120);
}

TEST_F(CorpusRestoreTest, RestorePositionsOffResetsToTopLeft)
{
    QSettings().setValue(Preferences::RestorePositions, false);
    writeAndOpen(/*block=*/10, /*col=*/3, /*scroll=*/120);

    Editor *ed = m_window->editor();
    ASSERT_NE(ed, nullptr);
    EXPECT_EQ(ed->textCursor().blockNumber(), 0);
    EXPECT_EQ(ed->textCursor().positionInBlock(), 0);
    EXPECT_EQ(ed->verticalScrollBar()->value(), 0);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
