#include <gtest/gtest.h>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QWebEnginePage>

#include "Editor.h"
#include "EditorTestHarness.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Preview.h"
#include "TestConfig.h"

class EmojiPreviewHarness : public EditorTestHarness
{
protected:
    void SetUp() override
    {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.remove(Preferences::CssFiles);
        settings.remove(Preferences::ActiveCssFile);
        settings.setValue(Preferences::ReopenLastSession, false);
        settings.setValue(Preferences::PreviewState, 1);
        settings.setValue(Preferences::EmojiMode,
            Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));

        QSettings().setValue(Preferences::FileAutoComplete, false);
        QSettings().setValue(Preferences::EmojiAutoComplete, false);
        QSettings().setValue(Preferences::LanguageAutoComplete, false);

        window = new MainWindow();
        window->show();
        QApplication::processEvents();
        editor = window->editor();
    }

    void TearDown() override
    {
        delete window;
        window = nullptr;
        editor = nullptr;
    }

    void waitForPreviewLoaded()
    {
        QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
        bool loaded = false;
        for (int i = 0; i < loadSpy.count(); ++i) {
            if (loadSpy.at(i).at(0).toBool()) {
                loaded = true;
                break;
            }
        }
        while (!loaded) {
            if (!loadSpy.wait(1000))
                break;
            if (loadSpy.last().at(0).toBool())
                loaded = true;
        }
        EXPECT_TRUE(loaded) << "preview page did not finish loading";

        // Allow the 1500ms heavy-JS timer (replaceEmoji / twemojiParse) to run
        QTest::qWait(2500);
    }

    QJsonObject runJs(const QString &script)
    {
        QVariant result;
        bool done = false;
        window->preview()->page()->runJavaScript(script, [&](const QVariant &r) {
            result = r;
            done = true;
        });
        for (int i = 0; i < 200 && !done; ++i)
            QTest::qWait(50);
        EXPECT_TRUE(done) << "runJavaScript callback did not fire";
        return QJsonDocument::fromJson(result.toString().toUtf8()).object();
    }

    MainWindow *window = nullptr;
};

TEST_F(EmojiPreviewHarness, ShortcodeRendersEmojiGlyphInPreview)
{
    typeText(":cucumber:");
    waitForPreviewLoaded();

    QJsonObject state = runJs(
        "(function(){"
        "var span = document.querySelector('.emoji-char');"
        "return JSON.stringify({"
        "hasShortcode: document.body.textContent.indexOf(':cucumber:') !== -1,"
        "hasEmoji: !!span && span.textContent.indexOf('\u{1F952}') !== -1"
        "});"
        "})()");

    EXPECT_FALSE(state.value("hasShortcode").toBool()) << "raw shortcode still visible";
    EXPECT_TRUE(state.value("hasEmoji").toBool()) << "emoji glyph not rendered";
}

TEST_F(EmojiPreviewHarness, ColorModeRendersTwemojiImage)
{
    QSettings().setValue(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Color));

    typeText(":cucumber:");
    waitForPreviewLoaded();

    QJsonObject state = runJs(
        "(function(){"
        "var img = document.querySelector('img.emoji');"
        "return JSON.stringify({"
        "hasShortcode: document.body.textContent.indexOf(':cucumber:') !== -1,"
        "hasImg: !!img,"
        "src: img ? img.src : '',"
        "naturalWidth: img ? img.naturalWidth : 0"
        "});"
        "})()");

    EXPECT_FALSE(state.value("hasShortcode").toBool()) << "raw shortcode still visible";
    EXPECT_TRUE(state.value("hasImg").toBool()) << "twemoji img element missing";
    EXPECT_TRUE(state.value("src").toString().contains("1f952"))
        << "unexpected twemoji source: " << state.value("src").toString().toStdString();
    EXPECT_GT(state.value("naturalWidth").toInt(), 0) << "twemoji SVG did not load";
}

TEST_F(EmojiPreviewHarness, CodeBlockKeepsShortcodeLiteral)
{
    typeText("`:cucumber:`");
    waitForPreviewLoaded();

    QJsonObject state = runJs(
        "(function(){"
        "return JSON.stringify({"
        "hasShortcode: document.body.textContent.indexOf(':cucumber:') !== -1,"
        "hasEmoji: !!document.querySelector('.emoji-char')"
        "});"
        "})()");

    EXPECT_TRUE(state.value("hasShortcode").toBool()) << "code span content changed";
    EXPECT_FALSE(state.value("hasEmoji").toBool()) << "emoji rendered inside code";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
