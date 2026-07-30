#include <gtest/gtest.h>
#include <QApplication>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QSettings>
#include <QTest>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"

class EditorInitialCssTest : public testing::Test {
protected:
    void SetUp() override {
        QSettings settings;
        settings.setValue(Preferences::ReopenLastSession, false);
        settings.remove(Preferences::EditorFontFamily);
        settings.remove(Preferences::EditorFontSize);
        settings.remove(Preferences::EditorPadding);
        settings.remove(Preferences::EditorLineHeight);
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
    EXPECT_TRUE(ss.contains("font-size: 16px"))
        << "Expected font-size: 16px in stylesheet, got: " << ss.toStdString();
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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("scribaTest");
    app.setApplicationName("scribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
