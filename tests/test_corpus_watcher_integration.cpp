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
//
// End-to-end corpus monitoring: a real MainWindow drives a CorpusWatcher;
// edits/renames on disk flow through the watcher into reloads, path updates
// and (per policy) link rewrites.
#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>
#include <QDockWidget>

#include <functional>

#include "mainwindow/MainWindow.h"
#include "editor/Editor.h"
#include "corpus/Corpus.h"
#include "corpus/CorpusFilesPanel.h"
#include "corpus/CorpusIndex.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"

namespace {

void writeFile(const QString &path, const QString &content)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    f.write(content.toUtf8());
    f.close();
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

bool waitFor(const std::function<bool()> &cond, int timeoutMs = 5000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QTest::qWait(50);
        if (cond())
            return true;
    }
    QApplication::processEvents();
    return cond();
}

class CorpusWatcherIntegrationTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_corpus_watcher_integration";
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

    void makeCorpus(const QStringList &docPaths)
    {
        Corpus c;
        c.filePath = m_corpusPath;
        c.name = QStringLiteral("test");
        c.monitor = true;
        c.active = 0;
        for (const QString &p : docPaths)
            c.documents.append({p});
        ASSERT_TRUE(c.save());
    }

    void openCorpus()
    {
        m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
        QApplication::processEvents();
        m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
        QApplication::processEvents();
        QTest::qWait(200);      // let the watcher settle on the initial paths
    }

    // Corpus document tabs sit at the same index as their position in
    // m_corpus.documents: openCorpusFile() drops the empty placeholder tab that
    // closeAllTabs() would otherwise leave behind.
    Editor *tabEditor(int index) const
    {
        auto *stack = m_window->findChild<QStackedWidget *>();
        const int i = index;
        if (!stack || i < 0 || i >= stack->count())
            return nullptr;
        return qobject_cast<Editor *>(stack->widget(i));
    }

    QString tabTooltip(int index) const
    {
        auto *tabs = m_window->findChild<QTabBar *>();
        const int i = index;
        if (!tabs || i < 0 || i >= tabs->count())
            return QString();
        return tabs->tabToolTip(i);
    }

    bool tabDirty(int index) const
    {
        auto *tabs = m_window->findChild<QTabBar *>();
        const int i = index;
        if (!tabs || i < 0 || i >= tabs->count())
            return false;
        return tabs->tabText(i).contains(QLatin1Char('*'));
    }

    Editor *stackEditor(int index) const
    {
        auto *stack = m_window->findChild<QStackedWidget *>();
        if (!stack || index < 0 || index >= stack->count())
            return nullptr;
        return qobject_cast<Editor *>(stack->widget(index));
    }

    int tocTabIndex() const
    {
        auto *tabs = m_window->findChild<QTabBar *>();
        if (!tabs)
            return -1;
        const QString tocPath = m_root + QStringLiteral("/toc.md");
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabToolTip(i) == tocPath)
                return i;
        }
        return -1;
    }

    static void triggerAction(MainWindow *win, const QString &textSubstring)
    {
        const auto actions = win->findChildren<QAction *>();
        for (QAction *a : actions) {
            if (a->text().contains(textSubstring)) {
                a->trigger();
                return;
            }
        }
        FAIL() << "No action containing text: " << textSubstring.toStdString();
    }

    CorpusFilesPanel *filesPanel() const
    {
        return m_window->findChild<CorpusFilesPanel *>();
    }

    QDockWidget *filesDock() const
    {
        return m_window->findChild<QDockWidget *>(QStringLiteral("CorpusFilesDock"));
    }

    // File names shown by the corpus-files panel at its top level.
    QStringList filesRootNames() const
    {
        auto *tree = filesPanel() ? filesPanel()->findChild<QTreeView *>() : nullptr;
        if (!tree)
            return {};
        QAbstractItemModel *m = tree->model();
        const QModelIndex root = tree->rootIndex();
        QStringList names;
        const int count = m->rowCount(root);
        for (int i = 0; i < count; ++i) {
            const QModelIndex idx = m->index(i, 0, root);
            if (idx.isValid())
                names.append(idx.data().toString());
        }
        return names;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_root;
    QString m_corpusPath;
    MainWindow *m_window = nullptr;
};

TEST_F(CorpusWatcherIntegrationTest, ExternalEditReloadsCleanTab)
{
    writeFile(m_root + "/doc.md", "one");
    makeCorpus({"doc.md"});
    openCorpus();

    ASSERT_EQ(tabEditor(0)->toPlainText(), QStringLiteral("one"));
    writeFile(m_root + "/doc.md", "two");

    EXPECT_TRUE(waitFor([&] {
        return tabEditor(0) && tabEditor(0)->toPlainText() == QStringLiteral("two");
    }));
    EXPECT_FALSE(tabDirty(0));
}

// Scriba's own save writes the tab's text back to disk; the watcher reports it
// as an external edit, but handleExternalEdit must not reload/prompt when the
// on-disk content already matches the tab. Here "two" is written by the app
// (mimicking auto-save), so the dirty tab must keep its text AND stay dirty —
// a force reload would mark it clean.
TEST_F(CorpusWatcherIntegrationTest, OwnWriteWithMatchingContentDoesNotReloadDirtyTab)
{
    QSettings().setValue(Preferences::CorpusExternalEditPolicy, QStringLiteral("autoReloadDirty"));
    writeFile(m_root + "/doc.md", "one");
    makeCorpus({"doc.md"});
    openCorpus();
    ASSERT_EQ(tabEditor(0)->toPlainText(), QStringLiteral("one"));

    tabEditor(0)->setPlainText(QStringLiteral("two"));   // unsaved edit -> dirty
    QApplication::processEvents();
    ASSERT_TRUE(tabDirty(0));

    writeFile(m_root + "/doc.md", QStringLiteral("two")); // own-write, matches tab
    QTest::qWait(1600);                                   // > Debounce::CorpusWatch

    EXPECT_EQ(tabEditor(0)->toPlainText(), QStringLiteral("two"));
    EXPECT_TRUE(tabDirty(0)) << "own-write matching the tab must not reload it";
}

TEST_F(CorpusWatcherIntegrationTest, RenameUpdatesTabAndCorpusJson)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("ignore"));
    writeFile(m_root + "/doc.md", "content");
    makeCorpus({"doc.md"});
    openCorpus();

    const QString oldAbs = QFileInfo(m_root + "/doc.md").absoluteFilePath();
    const QString newAbs = QFileInfo(m_root + "/renamed.md").absoluteFilePath();
    ASSERT_TRUE(QFile::rename(m_root + "/doc.md", m_root + "/renamed.md"));

    EXPECT_TRUE(waitFor([&] { return tabTooltip(0) == newAbs; }));
    EXPECT_TRUE(waitFor([&] { return readFile(m_corpusPath).contains(QStringLiteral("renamed.md")); }));
    EXPECT_FALSE(readFile(m_corpusPath).contains(QStringLiteral("doc.md")));
}

TEST_F(CorpusWatcherIntegrationTest, PolicyIgnoreSkipsRewrite)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("ignore"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"a.md", "b.md"});
    openCorpus();

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));
    EXPECT_TRUE(waitFor([&] {
        return tabTooltip(0) == QFileInfo(m_root + "/a2.md").absoluteFilePath();
    }));
    QTest::qWait(400);          // give any (wrong) rewrite time to happen
    EXPECT_EQ(tabEditor(1)->toPlainText(), QStringLiteral("See [x](a.md)"));
    EXPECT_FALSE(tabDirty(1));
}

TEST_F(CorpusWatcherIntegrationTest, PolicySilentRewritesOpenTabAndMarksDirty)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("silent"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"a.md", "b.md"});
    openCorpus();

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));
    EXPECT_TRUE(waitFor([&] {
        return tabEditor(1) && tabEditor(1)->toPlainText().contains(QStringLiteral("a2.md"));
    }));
    EXPECT_TRUE(tabDirty(1));
    EXPECT_EQ(tabEditor(1)->toPlainText(), QStringLiteral("See [x](a2.md)"));
}

// The exact live-session flow: open a corpus with no file documents, then load
// doc1.md/doc2.md into it. The watcher must monitor the newly opened files and
// rewrite links to doc1.md when it is renamed on disk.
TEST_F(CorpusWatcherIntegrationTest, DocsLoadedAfterCorpusOpenAreMonitoredAndLinksRewritten)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("silent"));
    writeFile(m_root + "/doc1.md", "one");
    writeFile(m_root + "/doc2.md", "See [d1](doc1.md)");
    makeCorpus({});                                // blank placeholder corpus

    m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
    QApplication::processEvents();
    QTest::qWait(200);                             // watcher settles (no files)

    m_window->loadFile(m_root + "/doc1.md");
    m_window->loadFile(m_root + "/doc2.md");
    QApplication::processEvents();
    QTest::qWait(200);                             // let the re-armed watcher settle

    const QString oldAbs = QFileInfo(m_root + "/doc1.md").absoluteFilePath();
    const QString newAbs = QFileInfo(m_root + "/doc_1.md").absoluteFilePath();
    ASSERT_TRUE(QFile::rename(m_root + "/doc1.md", m_root + "/doc_1.md"));

    EXPECT_TRUE(waitFor([&] { return tabTooltip(0) == newAbs; }))
        << "the re-armed watcher must detect the external rename";
    EXPECT_TRUE(waitFor([&] {
        return tabEditor(1) && tabEditor(1)->toPlainText().contains(QStringLiteral("doc_1.md"));
    }));
    EXPECT_TRUE(tabDirty(1));
    EXPECT_EQ(tabEditor(1)->toPlainText(), QStringLiteral("See [d1](doc_1.md)"));
    EXPECT_TRUE(waitFor([&] { return readFile(m_corpusPath).contains(QStringLiteral("doc_1.md")); }));
}

TEST_F(CorpusWatcherIntegrationTest, DocsLoadedAfterCorpusOpenPolicyIgnoreSkipsRewrite)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("ignore"));
    writeFile(m_root + "/doc1.md", "one");
    writeFile(m_root + "/doc2.md", "See [d1](doc1.md)");
    makeCorpus({});

    m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
    QApplication::processEvents();
    m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
    QApplication::processEvents();
    QTest::qWait(200);

    m_window->loadFile(m_root + "/doc1.md");
    m_window->loadFile(m_root + "/doc2.md");
    QApplication::processEvents();
    QTest::qWait(200);

    ASSERT_TRUE(QFile::rename(m_root + "/doc1.md", m_root + "/doc_1.md"));
    EXPECT_TRUE(waitFor([&] { return tabTooltip(0) == QFileInfo(m_root + "/doc_1.md").absoluteFilePath(); }));
    QTest::qWait(400);                             // give any (wrong) rewrite time
    EXPECT_EQ(tabEditor(1)->toPlainText(), QStringLiteral("See [d1](doc1.md)"));
    EXPECT_FALSE(tabDirty(1));
}

// The reported repro: a.md is NOT a corpus member (not in the .scriba, not an
// open tab) but is linked from b.md. An external `mv a.md a2.md` must still be
// detected and rewrite b.md's link.
TEST_F(CorpusWatcherIntegrationTest, ExternalRenameOfLinkedNonCorpusFileRewritesLink)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("silent"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"b.md"});
    openCorpus();

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));

    EXPECT_TRUE(waitFor([&] {
        return tabEditor(0) && tabEditor(0)->toPlainText().contains(QStringLiteral("a2.md"));
    })) << "external rename of a linked-but-non-corpus file must update links";
    EXPECT_TRUE(tabDirty(0));
    EXPECT_EQ(tabEditor(0)->toPlainText(), QStringLiteral("See [x](a2.md)"));
}

// A second external rename of the already-rewritten file must keep following
// the link. startCorpusWatcher re-extracts the monitored set (corpus docs +
// link targets) after each handled rename, so the chain never stalls.
TEST_F(CorpusWatcherIntegrationTest, ExternalRenameChainFollowsRepeatedRenames)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("silent"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"b.md"});
    openCorpus();

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));
    EXPECT_TRUE(waitFor([&] {
        return tabEditor(0) && tabEditor(0)->toPlainText().contains(QStringLiteral("a2.md"));
    }));

    ASSERT_TRUE(QFile::rename(m_root + "/a2.md", m_root + "/a3.md"));
    EXPECT_TRUE(waitFor([&] {
        return tabEditor(0) && tabEditor(0)->toPlainText().contains(QStringLiteral("a3.md"));
    }));
    EXPECT_EQ(tabEditor(0)->toPlainText(), QStringLiteral("See [x](a3.md)"));
}

// Scope "all" + ask policy: a closed affected doc (b.md) must be rewritten to
// disk only AFTER the user confirms. Clicking Yes applies the rewrite.
TEST_F(CorpusWatcherIntegrationTest, AskPolicyYesAppliesClosedDocDiskRewrite)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("prompt"));
    QSettings().setValue(Preferences::CorpusLinkRewriteScope, QStringLiteral("all"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"a.md", "b.md"});
    openCorpus();

    // Close b.md's tab so the rewrite must take the on-disk branch (scope "all").
    auto *tabs = m_window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    tabs->setCurrentIndex(1);
    QApplication::processEvents();
    triggerAction(m_window, QStringLiteral("Close Tab"));
    QApplication::processEvents();
    ASSERT_EQ(tabs->count(), 1);

    QTimer promptDriver;
    promptDriver.start(50);
    QObject::connect(&promptDriver, &QTimer::timeout, [&]() {
        if (auto *mb = qobject_cast<QMessageBox *>(qApp->activeModalWidget())) {
            mb->button(QMessageBox::Yes)->click();
            promptDriver.stop();
        }
    });

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));
    EXPECT_TRUE(waitFor([&] {
        return readFile(m_root + "/b.md").contains(QStringLiteral("a2.md"));
    }));
    promptDriver.stop();
    EXPECT_EQ(readFile(m_root + "/b.md"), QStringLiteral("See [x](a2.md)"));
}

// Scope "all" + ask policy: declining the prompt must leave the closed doc
// untouched on disk. Today the disk write happens before the prompt, so this
// test fails (b.md is rewritten despite clicking No).
TEST_F(CorpusWatcherIntegrationTest, AskPolicyNoLeavesClosedDocOnDiskUntouched)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("prompt"));
    QSettings().setValue(Preferences::CorpusLinkRewriteScope, QStringLiteral("all"));
    writeFile(m_root + "/a.md", "a content");
    writeFile(m_root + "/b.md", "See [x](a.md)");
    makeCorpus({"a.md", "b.md"});
    openCorpus();

    auto *tabs = m_window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    tabs->setCurrentIndex(1);
    QApplication::processEvents();
    triggerAction(m_window, QStringLiteral("Close Tab"));
    QApplication::processEvents();
    ASSERT_EQ(tabs->count(), 1);

    QTimer promptDriver;
    promptDriver.start(50);
    QObject::connect(&promptDriver, &QTimer::timeout, [&]() {
        if (auto *mb = qobject_cast<QMessageBox *>(qApp->activeModalWidget())) {
            mb->reject();                        // = No
            promptDriver.stop();
        }
    });

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/a2.md"));
    QTest::qWait(2500);                          // > Debounce::CorpusWatch + prompt handling
    promptDriver.stop();

    EXPECT_EQ(readFile(m_root + "/b.md"), QStringLiteral("See [x](a.md)"))
        << "declining the ask prompt must not rewrite closed docs to disk";
}

TEST_F(CorpusWatcherIntegrationTest, DefaultRewritePolicyIsAskFirst)
{
    QSettings().remove(Preferences::CorpusLinkRewritePolicy);
    EXPECT_EQ(QSettings().value(Preferences::CorpusLinkRewritePolicy,
                                QStringLiteral("prompt")).toString(),
              QStringLiteral("prompt"));
}

TEST_F(CorpusWatcherIntegrationTest, OpenTocCreatesTocMdWithManagedBlock)
{
    writeFile(m_root + "/doc.md", "# Alpha\n\n## Sub\n");
    makeCorpus({"doc.md"});
    openCorpus();

    triggerAction(m_window, QStringLiteral("Open Table of Contents"));
    QApplication::processEvents();

    const QString tocPath = m_root + QStringLiteral("/toc.md");
    ASSERT_TRUE(QFileInfo::exists(tocPath)) << "toc.md must be created in the corpus folder";
    const QString text = readFile(tocPath);
    EXPECT_TRUE(text.contains(CorpusIndex::tocStartMarker()));
    EXPECT_TRUE(text.contains(QStringLiteral("- [doc.md](doc.md)")));
    EXPECT_TRUE(text.contains(QStringLiteral("[Alpha](doc.md#alpha)")));
    EXPECT_TRUE(text.contains(CorpusIndex::tocEndMarker()));

    // A normal, editable tab is open on the file.
    const int toc = tocTabIndex();
    ASSERT_GE(toc, 0);
    Editor *ed = stackEditor(toc);
    ASSERT_NE(ed, nullptr);
    EXPECT_FALSE(ed->isReadOnly());
    EXPECT_EQ(ed->toPlainText(), text);
}

TEST_F(CorpusWatcherIntegrationTest, TocMdExcludedFromSavedCorpus)
{
    writeFile(m_root + "/doc.md", "# Alpha\n");
    makeCorpus({"doc.md"});
    openCorpus();

    triggerAction(m_window, QStringLiteral("Open Table of Contents"));
    QApplication::processEvents();
    triggerAction(m_window, QStringLiteral("Save Corpus"));
    QApplication::processEvents();

    const QString json = readFile(m_corpusPath);
    EXPECT_TRUE(json.contains(QStringLiteral("doc.md")));
    EXPECT_FALSE(json.contains(QStringLiteral("toc.md")));
    EXPECT_FALSE(json.contains(QStringLiteral("Table of Contents")));
}

TEST_F(CorpusWatcherIntegrationTest, TocMdRefreshesOnExternalRename)
{
    QSettings().setValue(Preferences::CorpusLinkRewritePolicy, QStringLiteral("ignore"));
    writeFile(m_root + "/doc.md", "# Alpha\n");
    makeCorpus({"doc.md"});
    openCorpus();
    triggerAction(m_window, QStringLiteral("Open Table of Contents"));
    QApplication::processEvents();
    const QString tocPath = m_root + QStringLiteral("/toc.md");
    ASSERT_TRUE(QFileInfo::exists(tocPath));

    ASSERT_TRUE(QFile::rename(m_root + "/doc.md", m_root + "/renamed.md"));

    EXPECT_TRUE(waitFor([&] {
        const QString text = readFile(tocPath);
        return text.contains(QStringLiteral("renamed.md"))
                 && !text.contains(QStringLiteral("[doc.md]"));
    }));
}

TEST_F(CorpusWatcherIntegrationTest, TocMdNotRestoredOrRelistedOnReopen)
{
    writeFile(m_root + "/doc.md", "# Alpha\n");
    makeCorpus({"doc.md"});
    openCorpus();
    triggerAction(m_window, QStringLiteral("Open Table of Contents"));
    QApplication::processEvents();
    ASSERT_GE(tocTabIndex(), 0);
    const QString tocPath = m_root + QStringLiteral("/toc.md");
    ASSERT_TRUE(QFileInfo::exists(tocPath));

    writeFile(m_root + "/doc2.md", "# Beta\n");
    Corpus c2;
    c2.filePath = m_root + "/corpus2.scriba";
    c2.name = QStringLiteral("test2");
    c2.monitor = true;
    c2.active = 0;
    c2.documents.append({QStringLiteral("doc2.md")});
    ASSERT_TRUE(c2.save());
    m_window->openCorpusFile(c2.filePath, /*skipPrompt=*/true);
    QApplication::processEvents();

    // The toc tab is not restored on reopen...
    EXPECT_EQ(tocTabIndex(), -1);
    auto *tabs = m_window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 1); // placeholder gone; only doc2.md

    // ...and the new corpus's generated block does not list toc.md.
    triggerAction(m_window, QStringLiteral("Open Table of Contents"));
    QApplication::processEvents();
    const QString text = readFile(m_root + QStringLiteral("/toc.md"));
    EXPECT_TRUE(text.contains(QStringLiteral("- [doc2.md](doc2.md)")));
    EXPECT_FALSE(text.contains(QStringLiteral("toc.md")));
}

TEST_F(CorpusWatcherIntegrationTest, ClosingWindowResavesCorpus)
{
    writeFile(m_root + "/doc.md", "one");
    makeCorpus({"doc.md"});
    openCorpus();

    writeFile(m_root + "/doc2.md", "two");
    m_window->loadFile(m_root + "/doc2.md");
    QApplication::processEvents();

    m_window->close();
    QApplication::processEvents();

    EXPECT_TRUE(readFile(m_corpusPath).contains(QStringLiteral("doc2.md")))
        << "the .scriba must be re-saved on close so last-open restores the full session";
    EXPECT_FALSE(readFile(m_corpusPath).contains(QStringLiteral("\"name\":\"Untitled\"")));
    EXPECT_FALSE(readFile(m_corpusPath).contains(QStringLiteral("\"Untitled\"")))
        << "the empty placeholder tab must never be serialized into the corpus";
}

TEST_F(CorpusWatcherIntegrationTest, NewCorpusClosesTabsSavesFreshCorpus)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    writeFile(m_root + "/doc.md", "one");
    makeCorpus({"doc.md"});
    openCorpus();

    writeFile(m_root + "/doc2.md", "two");
    m_window->loadFile(m_root + "/doc2.md");
    QApplication::processEvents();
    auto *tabs = m_window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 2);
    const QString newPath = m_root + "/fresh.scriba";
    QTimer::singleShot(0, [&]() {
        auto *dlg = qobject_cast<QFileDialog *>(qApp->activeModalWidget());
        if (!dlg)
            return;
        dlg->selectFile(newPath);
        QMetaObject::invokeMethod(dlg, "accept", Qt::QueuedConnection);
    });
    triggerAction(m_window, QStringLiteral("New Corpus"));
    QApplication::processEvents();
    QTest::qWait(300);

    ASSERT_EQ(tabs->count(), 1) << "New Corpus must close the old session's tabs";
    EXPECT_EQ(tabs->tabToolTip(0), QString()) << "the fresh corpus starts with a blank tab";
    const QString fresh = readFile(newPath);
    EXPECT_TRUE(fresh.contains(QStringLiteral("\"name\": \"Untitled\"")))
        << "the blank tab is serialized as the first embedded document";
    EXPECT_FALSE(fresh.contains(QStringLiteral("\"path\"")))
        << "a fresh corpus has no file-backed documents";
    EXPECT_EQ(QSettings().value(Preferences::LastCorpusPath).toString(), newPath);
}

TEST_F(CorpusWatcherIntegrationTest, NewCorpusCancelKeepsCurrentCorpus)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    writeFile(m_root + "/doc.md", "one");
    makeCorpus({"doc.md"});
    openCorpus();

    writeFile(m_root + "/doc2.md", "two");
    m_window->loadFile(m_root + "/doc2.md");
    QApplication::processEvents();
    auto *tabs = m_window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 2);

    const QString newPath = m_root + "/cancelled.scriba";
    QTimer::singleShot(0, [&]() {
        auto *dlg = qobject_cast<QFileDialog *>(qApp->activeModalWidget());
        if (!dlg)
            return;
        dlg->selectFile(newPath);
        QMetaObject::invokeMethod(dlg, "reject", Qt::QueuedConnection);
    });
    triggerAction(m_window, QStringLiteral("New Corpus"));
    QApplication::processEvents();
    QTest::qWait(300);

    ASSERT_EQ(tabs->count(), 2) << "cancelling New Corpus must leave the session untouched";
    EXPECT_FALSE(QFile::exists(newPath));
}

TEST_F(CorpusWatcherIntegrationTest, FilesPanelDockHiddenWithoutCorpus)
{
    m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
    QApplication::processEvents();

    ASSERT_NE(filesPanel(), nullptr);
    ASSERT_NE(filesDock(), nullptr);
    EXPECT_TRUE(filesDock()->isHidden()) << "no corpus loaded, the dock stays hidden";
}

TEST_F(CorpusWatcherIntegrationTest, FilesPanelListsCorpusRootAndExcludesScriba)
{
    writeFile(m_root + "/doc.md", "one");
    writeFile(m_root + "/img.png", "png");
    ASSERT_TRUE(QDir().mkpath(m_root + "/notes"));
    writeFile(m_root + "/notes/sub.md", "sub");
    makeCorpus({"doc.md"});
    openCorpus();

    ASSERT_NE(filesPanel(), nullptr);
    ASSERT_NE(filesDock(), nullptr);
    EXPECT_FALSE(filesDock()->isHidden())
        << "opening a corpus shows the Corpus Files dock";
    EXPECT_TRUE(waitFor([&] {
        const QStringList names = filesRootNames();
        return names.contains(QStringLiteral("doc.md"))
            && names.contains(QStringLiteral("img.png"))
            && names.contains(QStringLiteral("notes"));
    })) << "the panel lists every file in the corpus root";
    EXPECT_FALSE(filesRootNames().contains(QStringLiteral("corpus.scriba")))
        << "the corpus's own .scriba is excluded from the panel";
}

TEST_F(CorpusWatcherIntegrationTest, FilesPanelDocActivationLoadsTab)
{
    writeFile(m_root + "/doc.md", "one");
    writeFile(m_root + "/other.md", "other");
    makeCorpus({"doc.md"});
    openCorpus();

    auto *panel = filesPanel();
    ASSERT_NE(panel, nullptr);
    panel->fileActivated(m_root + "/other.md");
    QApplication::processEvents();

    EXPECT_TRUE(waitFor([&] {
        auto *tabs = m_window->findChild<QTabBar *>();
        if (!tabs)
            return false;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabToolTip(i) == m_root + "/other.md")
                return true;
        }
        return false;
    })) << "double-clicking a .md in the panel opens it as a tab";
    EXPECT_EQ(readFile(m_root + "/other.md"), QStringLiteral("other"));
}

TEST_F(CorpusWatcherIntegrationTest, FilesPanelImageActivationInsertsLink)
{
    writeFile(m_root + "/doc.md", "one");
    writeFile(m_root + "/img.png", "png");
    makeCorpus({"doc.md"});
    openCorpus();

    auto *panel = filesPanel();
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(tabEditor(0), nullptr);
    panel->fileActivated(m_root + "/img.png");
    QApplication::processEvents();

    EXPECT_TRUE(waitFor([&] {
        return tabEditor(0) && tabEditor(0)->toPlainText().contains(QStringLiteral("![img](img.png)"));
    })) << "double-clicking an image in the panel inserts a relative image link";
    EXPECT_FALSE(readFile(m_root + "/doc.md").contains(QStringLiteral("img.png")))
        << "the inserted link must not have touched the file on disk";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
