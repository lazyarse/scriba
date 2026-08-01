#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryFile>
#include <QSettings>
#include <QTabWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
#include "TestConfig.h"

class DirtyOnLoadTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_dirty_on_load";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        QSettings s;
        s.remove(Preferences::SessionData);
        s.setValue(Preferences::ReopenLastSession, false);
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

    static QString tabText(MainWindow *w) {
        QTabWidget *tabs = w->findChild<QTabWidget *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(0);
    }

    static QString activeTabText(MainWindow *w) {
        QTabWidget *tabs = w->findChild<QTabWidget *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(tabs->currentIndex());
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(DirtyOnLoadTest, LoadFileDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, ForceReloadDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->loadFile(tmpFile->fileName(), true);
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, TypingAfterLoadMarksTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->setPlainText("modified content");
    QApplication::processEvents();

    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, SessionRestoreDoesNotMarkTabDirty) {
    QSettings s;
    s.setValue(Preferences::ReopenLastSession, true);
    QJsonObject session;
    session["version"] = 1;
    session["files"] = QJsonArray{ tmpFile->fileName() };
    session["cursors"] = QJsonArray{};
    session["active"] = 0;
    s.setValue(Preferences::SessionData,
               QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));

    window = new MainWindow();
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
