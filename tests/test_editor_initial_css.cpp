#include <gtest/gtest.h>
#include <QApplication>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QSettings>
#include <QScrollBar>
#include <QTest>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
#include "TestConfig.h"

class EditorInitialCssTest : public testing::Test {
protected:
    void SetUp() override {
        QSettings settings;
        settings.setValue(Preferences::ReopenLastSession, false);
        settings.remove(Preferences::EditorFontFamily);
        settings.remove(Preferences::EditorFontSize);
        settings.remove(Preferences::EditorPadding);
        settings.remove(Preferences::EditorLineHeight);
        settings.setValue(Preferences::PreviewState, 1);
        settings.setValue(Preferences::SplitViewEditorMaxWidth, 0);
        settings.setValue(Preferences::SplitViewPreviewMaxWidth, 0);
    }

    void TearDown() override {
        delete window;
    }

    MainWindow *window = nullptr;
};

TEST_F(EditorInitialCssTest, EditorStylesheetHasPaddingFontSizeFontFamily) {
    window = new MainWindow();
    QApplication::processEvents();

    QString ss = window->editor()->styleSheet();

    EXPECT_TRUE(ss.contains("padding: 12px"))
        << "Expected padding: 12px in stylesheet, got: " << ss.toStdString();
    const QString expectedFontSize = QStringLiteral("font-size: %1pt").arg(Preferences::DefaultEditorFontSize);
    EXPECT_TRUE(ss.contains(expectedFontSize))
        << "Expected " << expectedFontSize.toStdString() << " in stylesheet, got: " << ss.toStdString();
    EXPECT_TRUE(ss.contains("font-family:"))
        << "Expected font-family in stylesheet, got: " << ss.toStdString();
}

TEST_F(EditorInitialCssTest, EditorLineHeightIsApplied) {
    window = new MainWindow();
    QApplication::processEvents();

    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::Start);
    QTextBlockFormat fmt = cursor.blockFormat();
    EXPECT_EQ(fmt.lineHeight(), 125);
    EXPECT_EQ(fmt.lineHeightType(), QTextBlockFormat::ProportionalHeight);
}

TEST_F(EditorInitialCssTest, SplitViewAutoLeavesFullWidth) {
    QSettings settings;
    settings.setValue(Preferences::SplitViewEditorMaxWidth, 0);
    window = new MainWindow();
    window->resize(1200, 800);
    window->show();
    QApplication::processEvents();
    QApplication::processEvents();

    EXPECT_EQ(window->editor()->contentMargins().right(), 0);
}

TEST_F(EditorInitialCssTest, SplitViewMaxWidthCentersEditorContent) {
    QSettings settings;
    settings.setValue(Preferences::SplitViewEditorMaxWidth, 400);
    window = new MainWindow();
    window->resize(1200, 800);
    window->show();
    QApplication::processEvents();
    QApplication::processEvents();

    Editor *ed = window->editor();
    QMargins m = ed->contentMargins();
    EXPECT_GT(m.right(), 0) << "content should be capped and centred when a max width is set";
    int contentWidth = ed->viewport()->width();
    EXPECT_GE(contentWidth, 400 - 25) << "content should be capped near the configured max width";
    EXPECT_LE(contentWidth, 400 + 25);
    EXPECT_GT(m.left(), m.right()) << "left margin includes the line-number gutter";

    window->resize(window->width() + 400, window->height());
    QApplication::processEvents();
    QApplication::processEvents();
    QMargins m2 = ed->contentMargins();
    EXPECT_GT(m2.right(), 0) << "content must remain capped after a resize";
    EXPECT_GE(ed->viewport()->width(), 400 - 25);
    EXPECT_LE(ed->viewport()->width(), 400 + 25);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
