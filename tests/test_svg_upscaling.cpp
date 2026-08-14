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
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QWebEnginePage>

#include "editor/Editor.h"
#include "EditorTestHarness.h"
#include "mainwindow/MainWindow.h"
#include "prefs/Preferences.h"
#include "preview/Preview.h"
#include "TestConfig.h"

class SvgUpscalingHarness : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.remove(Preferences::CssFiles);
        settings.remove(Preferences::ActiveCssFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);
        settings.setValue(Preferences::PreviewState, 3);
        settings.setValue(Preferences::EmojiMode,
            Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));

        QSettings().setValue(Preferences::FileAutoComplete, false);
        QSettings().setValue(Preferences::EmojiAutoComplete, false);
        QSettings().setValue(Preferences::LanguageAutoComplete, false);

        window = new MainWindow();
        window->resize(1280, 800);
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
    Editor *editor = nullptr;
};

TEST_F(SvgUpscalingHarness, SvgWithDimensionUpscalesInPreview)
{
    const QString readmePath =
        QDir(QCoreApplication::applicationDirPath()).filePath("../README.md");
    window->loadFile(readmePath);
    QApplication::processEvents();
    waitForPreviewLoaded();

    QJsonObject state = runJs(
        "(function(){"
        "var img = document.querySelector('img');"
        "if (!img) return JSON.stringify({found: false});"
        "var rect = img.getBoundingClientRect();"
        "return JSON.stringify({"
        "found: true,"
        "renderedWidth: rect.width,"
        "naturalWidth: img.naturalWidth"
        "});"
        "})()");

    EXPECT_TRUE(state.value("found").toBool()) << "no <img> found in preview";

    const int naturalWidth = state.value("naturalWidth").toInt();
    const int renderedWidth = state.value("renderedWidth").toInt();

    EXPECT_GT(naturalWidth, 0) << "SVG image did not load";
    EXPECT_GT(renderedWidth, naturalWidth)
        << "SVG with dimension suffix did not upscale";
}

TEST_F(SvgUpscalingHarness, SvgWithoutDimensionRendersNaturalSize)
{
    const QString readmePath =
        QDir(QCoreApplication::applicationDirPath()).filePath("../README.md");
    window->loadFile(readmePath);
    QApplication::processEvents();
    waitForPreviewLoaded();

    editor->selectAll();
    QApplication::processEvents();
    editor->insertPlainText(QStringLiteral("![alt](resources/icons/scriba.svg)"));
    QApplication::processEvents();
    QTest::qWait(1500);

    QJsonObject state = runJs(
        "(function(){"
        "var img = document.querySelector('img');"
        "if (!img) return JSON.stringify({found: false});"
        "var rect = img.getBoundingClientRect();"
        "return JSON.stringify({"
        "found: true,"
        "renderedWidth: rect.width,"
        "naturalWidth: img.naturalWidth"
        "});"
        "})()");

    EXPECT_TRUE(state.value("found").toBool()) << "no <img> found in preview";

    const int naturalWidth = state.value("naturalWidth").toInt();
    const int renderedWidth = state.value("renderedWidth").toInt();

    EXPECT_GT(naturalWidth, 0) << "SVG image did not load";
    EXPECT_LE(renderedWidth, naturalWidth)
        << "SVG without dimension suffix should not upscale";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
