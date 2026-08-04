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
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWebEnginePage>

#include "LinkValidator.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Preview.h"
#include "TestConfig.h"

static int s_argc = 1;
static char s_arg0[] = "test_anchor_navigation";
static char *s_argv[] = { s_arg0, nullptr };

namespace {

bool waitForLoaded(MainWindow *window, QSignalSpy *spy, int timeout = 15000)
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
        settings.setValue(Preferences::ReopenLastSession, false);
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
        settings.setValue(Preferences::ReopenLastSession, false);
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

    // The ids generateHeadingIds() assigned, in document order.
    QJsonArray headingIds()
    {
        QString result;
        window->preview()->page()->runJavaScript(
            "JSON.stringify(Array.from(document.querySelectorAll('h1,h2,h3,h4,h5,h6'))"
            ".map(function(h){return h.id}))",
            [&](const QVariant &r) { result = r.toString(); });
        QTest::qWait(500);
        return QJsonDocument::fromJson(result.toUtf8()).array();
    }

    // Polls window.scrollY until `min` is reached or `attempts` ticks elapse.
    double pollScrollY(double min, int attempts = 30)
    {
        double y = 0.0;
        for (int i = 0; i < attempts && y < min; ++i) {
            QTest::qWait(300);
            window->preview()->page()->runJavaScript(
                "document.body ? Math.round(window.scrollY) : 0",
                [&](const QVariant &r) { y = r.toDouble(); });
            QTest::qWait(100);
        }
        return y;
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

    // Allow the retry timer to run until it hits the target (300ms ticks).
    EXPECT_GT(pollScrollY(500.0, 30), 500);
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
    QString result;
    window->preview()->page()->runJavaScript(
        "(function(){var ov=document.getElementById('scriba-image-overlay');"
        "if(!ov)return JSON.stringify({display:'',src:''});"
        "var img=ov.querySelector('img');"
        "return JSON.stringify({display:ov.style.display,"
        "src:img?img.src:''});})()",
        [&](const QVariant &r) { result = r.toString(); });
    QTest::qWait(150);
    return result;
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

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
