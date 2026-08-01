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
