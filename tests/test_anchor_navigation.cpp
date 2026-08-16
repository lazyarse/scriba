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
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWebEnginePage>

#include "validation/LinkValidator.h"
#include "mainwindow/MainWindow.h"
#include "prefs/Preferences.h"
#include "preview/Preview.h"
#include "TestConfig.h"

#include <QAction>
#include <memory>

static int s_argc = 1;
static char s_arg0[] = "test_anchor_navigation";
static char *s_argv[] = { s_arg0, nullptr };

namespace {

// WebEngine can deliver a runJavaScript callback long after the call (the
// renderer stalls under load, or a navigation replaces the page). Capturing
// into a stack local and waiting a fixed qWait is therefore UB: a late
// callback writes into a dead frame. JsAnswer lives on the heap, so a late
// write is harmless, and runJsWait pumps the event loop until the callback
// for THIS invocation arrives (or the budget elapses).
struct JsAnswer
{
    bool answered = false;
    QVariant value;
};

static QVariant runJsWait(MainWindow *window, const QString &js, int timeoutMs = 3000)
{
    auto answer = std::make_shared<JsAnswer>();
    window->preview()->page()->runJavaScript(js, [answer](const QVariant &v) {
        answer->value = v;
        answer->answered = true;
    });
    for (int elapsed = 0; elapsed < timeoutMs && !answer->answered; elapsed += 50)
        QTest::qWait(50);
    return answer->value;
}

bool waitForLoaded(MainWindow *window, QSignalSpy *spy, int timeout = 60000)
{
    for (int i = 0; i < spy->count(); ++i)
        if (spy->at(i).at(0).toBool())
            return true;
    while (spy->wait(timeout)) {
        if (spy->last().at(0).toBool())
            return true;
    }
    return false;
}

class AnchorNavigationTest : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance())
            new QApplication(s_argc, s_argv);
    }

    void SetUp() override
    {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);
        settings.setValue(Preferences::PreviewState, 1);
        ASSERT_TRUE(m_dir.isValid());
        window = new MainWindow();
        window->resize(1200, 800);
        window->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete window;
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);
    }

    // Writes `content` into `name` inside the temp dir and opens it in the
    // main window, waiting for the preview to render.
    void loadDoc(const QString &name, const QByteArray &content)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(content);
        f.close();

        QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
        window->loadFile(path);
        QTest::qWait(300);
        ASSERT_TRUE(waitForLoaded(window, &loadSpy));
        // Let DOMContentLoaded run heading/id generation and the first render.
        QTest::qWait(2500);
    }

    // The ids generateHeadingIds() assigned, in document order. Re-polls until
    // the heavy pass has generated them (the page can answer "[]" while the
    // pass is still pending under load) or the budget runs out.
    QJsonArray headingIds()
    {
        const QString js = QStringLiteral(
            "JSON.stringify(Array.from(document.querySelectorAll('h1,h2,h3,h4,h5,h6'))"
            ".map(function(h){return h.id}))");
        QJsonArray ids;
        for (int attempt = 0; attempt < 40; ++attempt) {
            const QVariant v = runJsWait(window, js, 700);
            if (v.isValid() && !v.toString().isEmpty()) {
                ids = QJsonDocument::fromJson(v.toString().toUtf8()).array();
                if (!ids.isEmpty())
                    break;
            }
            QTest::qWait(250);
        }
        return ids;
    }

    // Polls window.scrollY until `min` is reached or `attempts` ticks elapse.
    double pollScrollY(double min, int attempts = 30)
    {
        double y = 0.0;
        for (int i = 0; i < attempts && y < min; ++i) {
            const QVariant v = runJsWait(window,
                QStringLiteral("document.body ? Math.round(window.scrollY) : 0"), 300);
            if (v.isValid())
                y = v.toDouble();
            if (y < min)
                QTest::qWait(200);
        }
        return y;
    }

    // Polls until the QWebChannel bridge is registered in the preview page.
    void waitForBridge()
    {
        for (int i = 0; i < 60; ++i) {
            const QVariant v = runJsWait(window,
                QStringLiteral("!!window.scribaBridge"), 100);
            if (v.toBool())
                return;
            QTest::qWait(100);
        }
    }

    QTemporaryDir m_dir;
    MainWindow *window = nullptr;
};

TEST_F(AnchorNavigationTest, HeadingIdsMatchCSlugGenerator)
{
    // Headings chosen to exercise unicode drops, punctuation drops, whitespace
    // collapse and duplicate -N suffixing — exactly like generateHeadingIds().
    loadDoc(QStringLiteral("ids.md"),
        "# Hello World\n"
        "## C++ 3.0\n"
        "### €uro!\n"
        "# Hello World\n"
        "# T_T\n"
        "#### Code: v2\n");

    QJsonArray ids = headingIds();

    // The C++ generator over the same headings, in document order.
    QSet<QString> expected;
    const QStringList headingTexts{
        QStringLiteral("Hello World"), QStringLiteral("C++ 3.0"),
        QStringLiteral("€uro!"), QStringLiteral("Hello World"),
        QStringLiteral("T_T"), QStringLiteral("Code: v2"),
    };
    for (const QString &t : headingTexts)
        LinkValidator::addHeadingSlugs(expected, t);

    QSet<QString> actual;
    for (const auto &v : ids)
        actual.insert(v.toString());

    EXPECT_EQ(expected, actual);
}

TEST_F(AnchorNavigationTest, SameDocumentClickScrollsToHeading)
{
    QString content;
    for (int i = 0; i < 120; ++i)
        content += QStringLiteral("Line %1.\n\n").arg(i);
    content += QStringLiteral("# The End\n\nFinal words. [jump](#the-end)\n");
    loadDoc(QStringLiteral("long.md"), content.toUtf8());

    ASSERT_GT(headingIds().size(), 0);

    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a[href=\"#the-end\"]');"
        "if(!a)return 'no-link';a.click();return 'clicked';})()",
        [](const QVariant &) {});
    QTest::qWait(1000);

    // Same-document jump happens in the click handler — no page reload, and
    // the heading sits far below the fold of a ~120-line body.
    EXPECT_GT(pollScrollY(500.0, 20), 500);
}

TEST_F(AnchorNavigationTest, CrossDocumentJumpScrollsAndSwitchesDoc)
{
    // Target doc must exist before a.md's link can jump to it.
    QString bContent = QStringLiteral("Start.\n\n");
    for (int i = 0; i < 60; ++i)
        bContent += QStringLiteral("Fill %1.\n\n").arg(i);
    bContent += QStringLiteral("# The Class\n\nEnd.\n");
    QFile b(m_dir.filePath(QStringLiteral("b.md")));
    ASSERT_TRUE(b.open(QIODevice::WriteOnly | QIODevice::Truncate));
    b.write(bContent.toUtf8());
    b.close();

    loadDoc(QStringLiteral("a.md"),
        "# In A\n\nPara.\n\n[Go to B](b.md#the-class)\n");

    // Click the cross-document link in the preview: it becomes a
    // scriba-open:file://... URL, MainWindow::urlChanged loads b.md and the
    // C++ retry timer scrolls to #the-class.
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a');if(!a)return false;"
        "a.click();return true;})()",
        [](const QVariant &) {});
    ASSERT_TRUE(waitForLoaded(window, &loadSpy));

    // Allow the retry timer to run until it hits the target (300ms ticks; the
    // C++ budget is ~30s, so poll that long to survive slow page loads).
    EXPECT_GT(pollScrollY(500.0, 100), 500);
}

TEST_F(AnchorNavigationTest, MissingHeadingStaysPutWithoutCrashing)
{
    loadDoc(QStringLiteral("doc.md"),
        "# Only Heading\n\n[text](#missing-heading)\n");

    QJsonArray ids = headingIds();
    ASSERT_EQ(1, ids.size());

    // No page reload, no jump; the retry timer gives up cleanly.
    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a[href=\"#missing-heading\"]');"
        "if(!a)return 'no-link';a.click();return 'clicked';})()",
        [](const QVariant &) {});
    QTest::qWait(3000);
    EXPECT_NEAR(0.0, pollScrollY(1.0, 5), 2.0);
}

// Returns the overlay's JSON state ({display, src}) or "no-overlay".
QString overlayState(MainWindow *window)
{
    return runJsWait(window,
        "(function(){var ov=document.getElementById('scriba-image-overlay');"
        "if(!ov)return JSON.stringify({display:'',src:''});"
        "var img=ov.querySelector('img');"
        "return JSON.stringify({display:ov.style.display,"
        "src:img?img.src:''});})()",
        2000).toString();
}

TEST_F(AnchorNavigationTest, ImageLinkShowsOverlayInPreview)
{
    const QString pngPath = m_dir.filePath(QStringLiteral("pic.png"));
    QFile png(pngPath);
    ASSERT_TRUE(png.open(QIODevice::WriteOnly | QIODevice::Truncate));
    png.write(QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=="));
    png.close();

    loadDoc(QStringLiteral("doc.md"), "# Img\n\n[see image](pic.png)\n");

    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a');if(!a)return 'no-link';"
        "a.click();return 'clicked';})()",
        [](const QVariant &) {});

    // The click goes through scriba-open to C++, which shows the overlay
    // instead of opening the file externally — poll until it appears.
    QJsonObject state;
    bool shown = false;
    for (int i = 0; i < 40 && !shown; ++i) {
        state = QJsonDocument::fromJson(overlayState(window).toUtf8()).object();
        shown = state.value("display").toString() == "flex";
        if (!shown) QTest::qWait(200);
    }
    ASSERT_TRUE(shown);

    const QString expectedSrc = QUrl::fromLocalFile(pngPath).toString();
    EXPECT_EQ(expectedSrc, state.value("src").toString());

    // Backdrop click (e.target === overlay) hides it again.
    window->preview()->page()->runJavaScript(
        "(function(){var ov=document.getElementById('scriba-image-overlay');"
        "if(ov)ov.click();return true;})()",
        [](const QVariant &) {});
    QTest::qWait(300);
    QJsonObject after =
        QJsonDocument::fromJson(overlayState(window).toUtf8()).object();
    EXPECT_EQ("none", after.value("display").toString());
}

TEST_F(AnchorNavigationTest, ImageLinkDoesNotResetPreviewScroll)
{
    // A real image the overlay can show.
    const QString pngPath = m_dir.filePath(QStringLiteral("pic.png"));
    QFile png(pngPath);
    ASSERT_TRUE(png.open(QIODevice::WriteOnly | QIODevice::Truncate));
    png.write(QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=="));
    png.close();

    // Long enough to scroll deep down; image link at the end.
    QString content;
    for (int i = 0; i < 150; ++i)
        content += QStringLiteral("Fill %1.\n\n").arg(i);
    content += QStringLiteral("[see image](pic.png)\n");
    loadDoc(QStringLiteral("long.md"), content.toUtf8());

    // Scroll the preview far down and record the position.
    window->preview()->page()->runJavaScript(
        "window.scrollTo(0, document.body.scrollHeight)");
    QTest::qWait(200);
    double before = 0.0;
    before = runJsWait(window,
        "document.body ? Math.round(window.scrollY) : 0", 1000).toDouble();
    ASSERT_GT(before, 500.0);

    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a');if(!a)return 'no-link';"
        "a.click();return 'clicked';})()",
        [](const QVariant &) {});

    // Wait for the lightbox to open.
    bool shown = false;
    for (int i = 0; i < 40 && !shown; ++i) {
        QJsonObject state =
            QJsonDocument::fromJson(overlayState(window).toUtf8()).object();
        shown = state.value("display").toString() == "flex";
        if (!shown) QTest::qWait(200);
    }
    ASSERT_TRUE(shown);

    // The fragment routing must not scroll the preview back to the top.
    double after = 0.0;
    after = runJsWait(window,
        "document.body ? Math.round(window.scrollY) : 0", 1000).toDouble();
    EXPECT_GT(after, 500.0);
}

TEST_F(AnchorNavigationTest, MissingImageLinkShowsNoOverlay)
{
    loadDoc(QStringLiteral("doc.md"),
        "# Img\n\n[missing](does-not-exist.png)\n");

    window->preview()->page()->runJavaScript(
        "(function(){var a=document.querySelector('a');if(!a)return 'no-link';"
        "a.click();return 'clicked';})()",
        [](const QVariant &) {});
    QTest::qWait(1500);

    QJsonObject state =
        QJsonDocument::fromJson(overlayState(window).toUtf8()).object();
    EXPECT_NE("flex", state.value("display").toString());
}

TEST_F(AnchorNavigationTest, FootnoteLinkHoverTitleShowsNoteText)
{
    loadDoc(QStringLiteral("foot.md"),
        "Water is H2O.[^1]\n\n[^1]: Two hydrogen atoms and one oxygen.\n");

    QString title;
    title = runJsWait(window,
        "(function(){var a=document.querySelector('a[href=\"#fn-1\"]');"
        "return a?(a.title||'no-title'):'no-link';})()",
        2000).toString();
    EXPECT_EQ(QStringLiteral("Two hydrogen atoms and one oxygen."), title);
}

TEST_F(AnchorNavigationTest, FootnoteBackrefGetsNoTitle)
{
    loadDoc(QStringLiteral("foot2.md"),
        "Note here.[^a]\n\n[^a]: Body text.\n");

    QString title;
    title = runJsWait(window,
        "(function(){var a=document.querySelector('.footnote-backref');"
        "return a?(a.title||'no-title'):'no-backref';})()",
        2000).toString();
    EXPECT_EQ(QStringLiteral("no-title"), title);
}

// Regression: clicking a local-file link opens the target in a new tab;
// after closing that tab, clicking the same link again must reopen it.
// The old #scriba-open: fragment mechanism could leave a stale URL so the
// repeated click was a no-op; the QWebChannel bridge routes every click to
// C++ directly.
TEST_F(AnchorNavigationTest, LinkReopensAfterClosingTabWorks)
{
    QFile b(m_dir.filePath(QStringLiteral("b.md")));
    ASSERT_TRUE(b.open(QIODevice::WriteOnly | QIODevice::Truncate));
    b.write("# In B\n\nBody.\n");
    b.close();

    loadDoc(QStringLiteral("a.md"),
        "# In A\n\nPara.\n\n[Go to B](b.md)\n");
    waitForBridge();

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 1);

    // Click the a.md link. After a tab close the preview re-renders the a.md
    // content asynchronously, so the anchor may not exist yet — poll until the
    // click actually lands (the app under load takes a while to repaint).
    auto clickLink = [&]() {
        for (int i = 0; i < 80; ++i) {
            const QVariant v = runJsWait(window,
                "(function(){var a=document.querySelector('a');"
                "if(!a)return 'no-link';a.click();return 'clicked';})()",
                500);
            if (v.toString() == "clicked")
                return;
            QTest::qWait(100);
        }
    };

    // Close the current tab via the menu action rather than a Ctrl+W
    // keyClick: after clicking a link the preview page owns focus, and
    // QtWebEngine can swallow the key before WindowShortcut dispatch.
    auto closeTab = [&]() {
        const auto actions = window->findChildren<QAction *>();
        for (QAction *a : actions) {
            if (a->text().contains(QStringLiteral("Close Tab"))) {
                a->trigger();
                return;
            }
        }
    };

    for (int round = 0; round < 3; ++round) {
        SCOPED_TRACE("round " + QString::number(round).toStdString());

        clickLink();
        bool opened = false;
        for (int i = 0; i < 40 && !opened; ++i) {
            QTest::qWait(150);
            opened = tabBar->count() > 1;
        }
        ASSERT_TRUE(opened);

        closeTab();
        bool closed = false;
        for (int i = 0; i < 40 && !closed; ++i) {
            QTest::qWait(150);
            closed = tabBar->count() == 1;
        }
        ASSERT_TRUE(closed);
    }
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
