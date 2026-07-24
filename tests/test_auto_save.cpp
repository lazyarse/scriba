#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryFile>
#include <QSettings>
#include <QTextCursor>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"

class AutoSaveTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_auto_save";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        QSettings s;
        s.remove(Preferences::LastOpenedFile);
        s.setValue(Preferences::ReopenLastFile, false);
        s.setValue(Preferences::AutoSaveOnExit, false);
        s.setValue(Preferences::AutoSaveInterval, 0);

        tmpFile = new QTemporaryFile();
        ASSERT_TRUE(tmpFile->open());
        tmpFile->write("original content\n");
        tmpFile->close();
    }

    void TearDown() override {
        delete window;
        delete tmpFile;
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(AutoSaveTest, NoFileLoadedDoesNothing) {
    window = new MainWindow();
    QApplication::processEvents();

    window->editor()->setPlainText("unsaved content");
    window->autoSave();
}

TEST_F(AutoSaveTest, SavesContentToLoadedFile) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("modified content");
    window->autoSave();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "modified content");
}

TEST_F(AutoSaveTest, SaveOnExitWritesFile) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, true);

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("exit content");
    window->close();
    QApplication::processEvents();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "exit content");
}

TEST_F(AutoSaveTest, SaveOnExitDoesNotWriteWhenDisabled) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, false);

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("should not be saved");
    window->close();
    QApplication::processEvents();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "original content\n");
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("ScribaTest");
    app.setApplicationName("ScribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
