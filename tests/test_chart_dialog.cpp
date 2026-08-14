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
#include <QComboBox>
#include <QCheckBox>
#include "charts/ChartDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_chart_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class ChartDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    static QJsonObject specFromGenerated(const QString &generated) {
        int start = generated.indexOf(QStringLiteral("```ec\n")) + 5;
        int end = generated.indexOf(QStringLiteral("\n```"), start);
        QString json = generated.mid(start, end - start);
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
        EXPECT_EQ(err.error, QJsonParseError::NoError);
        return doc.object();
    }

    ChartDialog dlg;
};

TEST_F(ChartDialogTest, GeneratedSpecWrappedInEcFence) {
    QString spec = dlg.generatedSpec();
    EXPECT_TRUE(spec.startsWith("\n```ec\n"));
    EXPECT_TRUE(spec.trimmed().endsWith("```"));
}

TEST_F(ChartDialogTest, DefaultSpecIsValidJson) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_TRUE(obj.contains("series"));
}

TEST_F(ChartDialogTest, DefaultSpecHasBarSeries) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    ASSERT_EQ(series.size(), 1);
    EXPECT_EQ(series[0].toObject()["type"].toString(), "bar");
}

TEST_F(ChartDialogTest, DefaultSpecHasXAxisData) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonObject x = obj["xAxis"].toObject();
    EXPECT_EQ(x["type"].toString(), "category");
    EXPECT_EQ(x["data"].toArray().size(), 3);
}

TEST_F(ChartDialogTest, DefaultSpecHasSeriesData) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    ASSERT_EQ(series.size(), 1);
    EXPECT_EQ(series[0].toObject()["data"].toArray().size(), 3);
}

TEST_F(ChartDialogTest, AnimationDisabledByDefault) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_EQ(obj["animation"].toBool(), false);
}

TEST_F(ChartDialogTest, AnimationCheckedOmitsAnimationKey) {
    auto *animate = dlg.findChild<QCheckBox *>("animateCheck");
    ASSERT_NE(animate, nullptr);
    animate->setChecked(true);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_FALSE(obj.contains("animation"));
}

TEST_F(ChartDialogTest, PieChartHasNoAxes) {
    auto *combo = dlg.findChild<QComboBox *>();
    ASSERT_NE(combo, nullptr);
    int pieIdx = combo->findText("Pie");
    ASSERT_GE(pieIdx, 0);
    combo->setCurrentIndex(pieIdx);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_FALSE(obj.contains("xAxis"));
    EXPECT_FALSE(obj.contains("yAxis"));
    QJsonArray series = obj["series"].toArray();
    ASSERT_EQ(series.size(), 1);
    EXPECT_EQ(series[0].toObject()["type"].toString(), "pie");
    EXPECT_EQ(series[0].toObject()["data"].toArray().size(), 3);
}

TEST_F(ChartDialogTest, ParseCsvData) {
    QString csv = "category,value\nA,10\nB,20\nC,30";
    QList<QMap<QString, QString>> rows = dlg.parseCsvData(csv);
    ASSERT_EQ(rows.size(), 3);
    EXPECT_EQ(rows[0]["category"], "A");
    EXPECT_EQ(rows[0]["value"], "10");
    EXPECT_EQ(rows[2]["category"], "C");
    EXPECT_EQ(rows[2]["value"], "30");
}

TEST_F(ChartDialogTest, ParseJsonData) {
    QString json = R"([{"category":"X","value":"42"},{"category":"Y","value":"99"}])";
    QList<QMap<QString, QString>> rows = dlg.parseJsonData(json);
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0]["category"], "X");
    EXPECT_EQ(rows[0]["value"], "42");
    EXPECT_EQ(rows[1]["category"], "Y");
    EXPECT_EQ(rows[1]["value"], "99");
}

TEST_F(ChartDialogTest, ParseJsonDataInvalidReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseJsonData("not json");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(ChartDialogTest, ParseJsonDataNotArrayReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseJsonData(R"({"key":"value"})");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(ChartDialogTest, ParseCsvDataEmptyReturnsEmpty) {
    QList<QMap<QString, QString>> rows = dlg.parseCsvData("");
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(ChartDialogTest, NumericXAxisUsesValueType) {
    auto *xCombo = dlg.findChildren<QComboBox *>().first();
    for (QComboBox *combo : dlg.findChildren<QComboBox *>()) {
        if (combo->currentText() == "Column 1") {
            xCombo = combo;
            break;
        }
    }
    ASSERT_NE(xCombo, nullptr);
    int idx = xCombo->findText("Column 2");
    ASSERT_GE(idx, 0);
    xCombo->setCurrentIndex(idx);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonObject x = obj["xAxis"].toObject();
    EXPECT_EQ(x["type"].toString(), "value");
}

TEST_F(ChartDialogTest, PreviewHtmlDefersInitUntilContainerHasWidth) {
    QString html = ChartDialog::previewPageHtml("{}");
    int guardPos = html.indexOf(QStringLiteral("vis.clientWidth>0"));
    int initPos = html.indexOf(QStringLiteral("echarts.init(vis,"));
    EXPECT_GT(guardPos, 0);
    EXPECT_GT(initPos, guardPos)
        << "echarts.init must only run after the container reports a real width, "
           "otherwise container-sized charts collapse to zero width";
}

TEST_F(ChartDialogTest, PreviewHtmlUsesSvgRenderer) {
    QString html = ChartDialog::previewPageHtml("{}");
    EXPECT_TRUE(html.contains(QStringLiteral("renderer:'svg'")));
}

TEST_F(ChartDialogTest, PreviewHtmlHasValidContainerWidthCss) {
    QString html = ChartDialog::previewPageHtml("{}");
    EXPECT_TRUE(html.contains(QStringLiteral("#vis{width:100%;")))
        << "QString::arg() does not unescape %%, so the rule must be written "
           "with a single % or the CSS is invalid and the container collapses "
           "to its content width";
    EXPECT_FALSE(html.contains(QStringLiteral("width:100%%")));
}
