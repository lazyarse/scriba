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
#include <QJsonArray>
#include <QCheckBox>
#include "VegaLiteDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_vegalite_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class VegaLiteDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    VegaLiteDialog dlg;
};

TEST_F(VegaLiteDialogTest, DefaultSpecIsValidJson) {
    QString spec = dlg.generatedSpec();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8(), &err);
    EXPECT_EQ(err.error, QJsonParseError::NoError);
    EXPECT_TRUE(doc.isObject());
}

TEST_F(VegaLiteDialogTest, DefaultSpecHasBarMark) {
    QString spec = dlg.generatedSpec();
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8());
    QJsonObject obj = doc.object();
    EXPECT_EQ(obj["mark"].toString(), "bar");
}

TEST_F(VegaLiteDialogTest, DefaultSpecHasDataKey) {
    QString spec = dlg.generatedSpec();
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8());
    QJsonObject obj = doc.object();
    EXPECT_TRUE(obj.contains("data"));
    EXPECT_TRUE(obj["data"].toObject().contains("values"));
}

TEST_F(VegaLiteDialogTest, ParseCsvData) {
    QString csv = "category,value\nA,10\nB,20\nC,30";
    QList<QMap<QString, QString>> rows = dlg.parseCsvData(csv);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_EQ(rows[0]["category"], "A");
    EXPECT_EQ(rows[0]["value"], "10");
    EXPECT_EQ(rows[2]["category"], "C");
    EXPECT_EQ(rows[2]["value"], "30");
}

TEST_F(VegaLiteDialogTest, ParseJsonData) {
    QString json = R"([{"category":"X","value":"42"},{"category":"Y","value":"99"}])";
    QList<QMap<QString, QString>> rows = dlg.parseJsonData(json);
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0]["category"], "X");
    EXPECT_EQ(rows[0]["value"], "42");
    EXPECT_EQ(rows[1]["category"], "Y");
    EXPECT_EQ(rows[1]["value"], "99");
}

TEST_F(VegaLiteDialogTest, ParseJsonDataInvalidReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseJsonData("not json");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(VegaLiteDialogTest, ParseJsonDataNotArrayReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseJsonData(R"({"key":"value"})");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(VegaLiteDialogTest, ParseCsvDataEmptyReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseCsvData("");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(VegaLiteDialogTest, FillWidthUncheckedHasNoContainerSize) {
    auto *fill = dlg.findChild<QCheckBox *>("fillWidthCheck");
    ASSERT_NE(fill, nullptr);
    fill->setChecked(false);
    QJsonObject obj = QJsonDocument::fromJson(dlg.generatedSpec().toUtf8()).object();
    EXPECT_FALSE(obj.contains("width"));
    EXPECT_FALSE(obj.contains("height"));
}

TEST_F(VegaLiteDialogTest, FillWidthCheckedSetsWidthContainerOnly) {
    auto *fill = dlg.findChild<QCheckBox *>("fillWidthCheck");
    ASSERT_NE(fill, nullptr);
    fill->setChecked(true);
    QJsonObject obj = QJsonDocument::fromJson(dlg.generatedSpec().toUtf8()).object();
    EXPECT_EQ(obj["width"].toString(), "container");
    EXPECT_FALSE(obj.contains("height"));
}

TEST_F(VegaLiteDialogTest, PreviewHtmlDefersEmbedUntilContainerHasWidth) {
    QString html = VegaLiteDialog::previewPageHtml("{}");
    int guardPos = html.indexOf(QStringLiteral("vis.clientWidth>0"));
    int embedPos = html.indexOf(QStringLiteral("vegaEmbed(vis,"));
    EXPECT_GT(guardPos, 0);
    EXPECT_GT(embedPos, guardPos)
        << "vegaEmbed must only run after the container reports a real width, "
           "otherwise container-sized charts collapse to zero width";
}

TEST_F(VegaLiteDialogTest, PreviewHtmlHasValidContainerWidthCss) {
    QString html = VegaLiteDialog::previewPageHtml("{}");
    EXPECT_TRUE(html.contains(QStringLiteral("#vis{width:100%;}")))
        << "QString::arg() does not unescape %%, so the rule must be written "
           "with a single % or the CSS is invalid and the container collapses "
           "to its content width";
    EXPECT_FALSE(html.contains(QStringLiteral("width:100%%")));
}
