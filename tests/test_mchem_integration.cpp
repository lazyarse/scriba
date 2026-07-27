#include <gtest/gtest.h>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>

static QString readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

static QString resourcePath(const QString &name) {
    return QDir(RESOURCE_DIR).filePath(name);
}

TEST(MchemResources, MchemJsExists) {
    QFile f(resourcePath("contrib/mhchem.min.js"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_GT(f.size(), 10000);
}

TEST(MchemResources, MchemJsContainsCeCommand) {
    QString js = readFile(resourcePath("contrib/mhchem.min.js"));
    EXPECT_TRUE(js.contains("ce"));
}

TEST(MchemQrc, QrcListsMchemFile) {
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("contrib/mhchem.min.js"));
}

TEST(MchemQrc, QrcStillListsAutoRender) {
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("contrib/auto-render.min.js"));
}

TEST(MchemQrc, QrcStillListsKatex) {
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("katex.min.js"));
    EXPECT_TRUE(qrc.contains("katex.min.css"));
}

TEST(MchemIntegration, MchemLargerThanExpected) {
    QFile f(resourcePath("contrib/mhchem.min.js"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_GT(f.size(), 20000);
}
