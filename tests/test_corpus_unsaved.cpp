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

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>

#include "mainwindow/MainWindow.h"
#include "editor/Editor.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"

namespace {

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

void writeFile(const QString &path, const QString &content)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    f.write(content.toUtf8());
    f.close();
}

// Stubs the Save-As dialog so tests can simulate success (a real path) or
// cancel (empty string) without a modal dialog.
class TestMainWindow : public MainWindow
{
public:
    QString saveAsResult;
    int saveAsCalls = 0;

protected:
    QString saveAsDialogPath() override
    {
        ++saveAsCalls;
        return saveAsResult;
    }
};

class CorpusUnsavedTest : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_corpus_unsaved";
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

        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_root = m_dir->path();
        m_corpusPath = m_root + "/corpus.scriba";
    }

    void TearDown() override
    {
        delete m_window;
        m_window = nullptr;
    }

    // A corpus holding a single embedded (untitled) document. openCorpusFile
    // restores it as an untitled tab with content, not dirty.
    void makeEmbeddedCorpus()
    {
        writeFile(m_corpusPath, QStringLiteral(
            "{\n"
            "  \"version\": 1,\n"
            "  \"active\": 0,\n"
            "  \"documents\": [\n"
            "    {\n"
            "      \"content\": \"hello embedded world\",\n"
            "      \"name\": \"Untitled\",\n"
            "      \"cursor\": { \"block\": 0, \"col\": 0 },\n"
            "      \"scroll\": 0,\n"
            "      \"folds\": []\n"
            "    }\n"
            "  ]\n"
            "}\n"));
    }

    TestMainWindow *m_window = nullptr;
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_root;
    QString m_corpusPath;
};

// "Save Corpus" triggers saveCorpusAction(); objectName set for this reason.
void triggerSaveCorpus(MainWindow *w)
{
    auto *act = w->findChild<QAction *>(QStringLiteral("action-save-corpus"));
    ASSERT_TRUE(act);
    act->trigger();
    QApplication::processEvents();
}

TEST_F(CorpusUnsavedTest, PromptModeSavesUntitledToFileAndStoresByPath)
{
    QSettings().setValue(Preferences::CorpusUnsavedDocs, QStringLiteral("prompt"));
    makeEmbeddedCorpus();

    m_window = new TestMainWindow;
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
    ASSERT_EQ(m_window->editor()->toPlainText(), QStringLiteral("hello embedded world"));

    const QString savePath = m_root + "/notes.md";
    m_window->saveAsResult = savePath;

    triggerSaveCorpus(m_window);

    // The untitled content landed in a real file...
    EXPECT_EQ(readFile(savePath), QStringLiteral("hello embedded world"));
    EXPECT_EQ(m_window->saveAsCalls, 1);

    // ...and the corpus now stores it by path, not as embedded content.
    const QString corpusJson = readFile(m_corpusPath);
    EXPECT_TRUE(corpusJson.contains(QStringLiteral("\"path\": \"notes.md\""))) << corpusJson.toStdString();
    EXPECT_FALSE(corpusJson.contains(QStringLiteral("hello embedded world"))) << corpusJson.toStdString();
}

TEST_F(CorpusUnsavedTest, CancelAbortsCorpusSave)
{
    QSettings().setValue(Preferences::CorpusUnsavedDocs, QStringLiteral("prompt"));
    makeEmbeddedCorpus();

    m_window = new TestMainWindow;
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);

    const QString before = readFile(m_corpusPath);
    m_window->saveAsResult = QString();      // cancel the Save-As dialog

    triggerSaveCorpus(m_window);

    // Corpus file untouched; tab is still untitled with its content.
    EXPECT_EQ(readFile(m_corpusPath), before);
    EXPECT_EQ(m_window->saveAsCalls, 1);
    EXPECT_EQ(m_window->editor()->toPlainText(), QStringLiteral("hello embedded world"));
}

TEST_F(CorpusUnsavedTest, EmbedModeDefaultKeepsEmbedding)
{
    makeEmbeddedCorpus();                     // no preference set: "embed" default

    m_window = new TestMainWindow;
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);

    triggerSaveCorpus(m_window);

    EXPECT_EQ(m_window->saveAsCalls, 0) << "embed mode must not prompt";
    const QString corpusJson = readFile(m_corpusPath);
    EXPECT_TRUE(corpusJson.contains(QStringLiteral("hello embedded world"))) << corpusJson.toStdString();
}

TEST_F(CorpusUnsavedTest, CloseInPromptModeSavesUntitled)
{
    QSettings().setValue(Preferences::CorpusUnsavedDocs, QStringLiteral("prompt"));
    makeEmbeddedCorpus();

    m_window = new TestMainWindow;
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);

    const QString savePath = m_root + "/notes.md";
    m_window->saveAsResult = savePath;

    m_window->close();
    QApplication::processEvents();

    EXPECT_EQ(readFile(savePath), QStringLiteral("hello embedded world"));
    EXPECT_EQ(m_window->saveAsCalls, 1);
}

TEST_F(CorpusUnsavedTest, CloseInPromptModeCancelAbortsClose)
{
    QSettings().setValue(Preferences::CorpusUnsavedDocs, QStringLiteral("prompt"));
    makeEmbeddedCorpus();

    m_window = new TestMainWindow;
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);

    const QString savePath = m_root + "/notes.md";
    m_window->saveAsResult = QString();      // cancel the Save-As dialog

    m_window->close();
    QApplication::processEvents();

    EXPECT_FALSE(QFile::exists(savePath));
    EXPECT_EQ(m_window->saveAsCalls, 1);
    // The close was ignored: the untitled content is still live in the editor.
    EXPECT_EQ(m_window->editor()->toPlainText(), QStringLiteral("hello embedded world"));
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
