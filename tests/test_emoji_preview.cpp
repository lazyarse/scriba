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
        settings.setValue(Preferences::ReopenLastCorpus, false);
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
            if (!loadSpy.wait(5000))
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
