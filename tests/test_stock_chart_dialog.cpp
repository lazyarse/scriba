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
#include <cmath>
#include "charts/StockChartDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_stock_chart_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class StockChartDialogTest : public ::testing::Test {
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

    StockChartDialog dlg;
};

TEST_F(StockChartDialogTest, GeneratedSpecWrappedInEcFence) {
    QString spec = dlg.generatedSpec();
    EXPECT_TRUE(spec.startsWith("\n```ec\n"));
    EXPECT_TRUE(spec.trimmed().endsWith("```"));
}

TEST_F(StockChartDialogTest, HasCandlestickSeries) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    ASSERT_FALSE(series.isEmpty());
    EXPECT_EQ(series[0].toObject()["type"].toString(), "candlestick");
    EXPECT_EQ(series[0].toObject()["name"].toString(), "OHLC");
}

TEST_F(StockChartDialogTest, CandlestickDataIsOpenCloseLowHigh) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    QJsonArray data = series[0].toObject()["data"].toArray();
    ASSERT_FALSE(data.isEmpty());
    QJsonArray first = data[0].toArray();
    ASSERT_EQ(first.size(), 4);
    EXPECT_DOUBLE_EQ(first[0].toDouble(), 152.4);  // open
    EXPECT_DOUBLE_EQ(first[1].toDouble(), 153.9);  // close
    EXPECT_DOUBLE_EQ(first[2].toDouble(), 151.2);  // low
    EXPECT_DOUBLE_EQ(first[3].toDouble(), 154.8);  // high
}

TEST_F(StockChartDialogTest, VolumeSeriesPresentByDefault) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    bool found = false;
    for (const QJsonValue &v : series) {
        QJsonObject s = v.toObject();
        if (s["type"].toString() == "bar" && s["name"].toString() == "Volume")
            found = true;
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(obj.contains("dataZoom"));
    QJsonArray grid = obj["grid"].toArray();
    EXPECT_EQ(grid.size(), 2);
}

TEST_F(StockChartDialogTest, MaSeriesMatchComputedAverages) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();

    QJsonArray closes;
    for (const QJsonValue &v : series[0].toObject()["data"].toArray())
        closes.append(v.toArray()[1].toDouble());

    for (const QJsonValue &v : series) {
        QJsonObject s = v.toObject();
        QString name = s["name"].toString();
        if (!name.startsWith("MA")) continue;
        int period = name.mid(2).toInt();
        QJsonArray maData = s["data"].toArray();
        ASSERT_EQ(maData.size(), closes.size()) << "MA series must align with candles";
        for (int i = 0; i < closes.size(); ++i) {
            if (i < period - 1) {
                EXPECT_TRUE(maData[i].isNull()) << name.toStdString() << "[" << i << "] must be null";
            } else {
                double sum = 0;
                for (int j = 0; j < period; ++j)
                    sum += closes[i - j].toDouble();
                EXPECT_NEAR(maData[i].toDouble(), sum / period, 1e-9)
                    << name.toStdString() << "[" << i << "]";
            }
        }
    }
}

TEST_F(StockChartDialogTest, AnimationDisabledByDefault) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_EQ(obj["animation"].toBool(), false);
}

TEST_F(StockChartDialogTest, AnimationCheckedOmitsAnimationKey) {
    auto *animate = dlg.findChild<QCheckBox *>("animateCheck");
    ASSERT_NE(animate, nullptr);
    animate->setChecked(true);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_FALSE(obj.contains("animation"));
}

TEST_F(StockChartDialogTest, UncheckMaRemovesSeries) {
    auto *ma5 = dlg.findChild<QCheckBox *>("");
    for (QCheckBox *check : dlg.findChildren<QCheckBox *>()) {
        if (check->text() == "MA5") ma5 = check;
        if (check->text() == "MA20") check->setChecked(false);
    }
    ASSERT_NE(ma5, nullptr);
    ma5->setChecked(false);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    for (const QJsonValue &v : series) {
        QString name = v.toObject()["name"].toString();
        EXPECT_NE(name, "MA5");
        EXPECT_NE(name, "MA20");
    }
}

TEST_F(StockChartDialogTest, MovingAveragePureFunction) {
    QList<double> values = {1, 2, 3, 4, 5, 6};
    QList<double> ma3 = StockChartDialog::movingAverage(values, 3);
    ASSERT_EQ(ma3.size(), 6);
    EXPECT_TRUE(std::isnan(ma3[0]));
    EXPECT_TRUE(std::isnan(ma3[1]));
    EXPECT_DOUBLE_EQ(ma3[2], 2.0);
    EXPECT_DOUBLE_EQ(ma3[3], 3.0);
    EXPECT_DOUBLE_EQ(ma3[4], 4.0);
    EXPECT_DOUBLE_EQ(ma3[5], 5.0);
}

TEST_F(StockChartDialogTest, PreviewHtmlDefersInitUntilContainerHasWidth) {
    QString html = StockChartDialog::previewPageHtml("{}");
    int guardPos = html.indexOf(QStringLiteral("vis.clientWidth>0"));
    int initPos = html.indexOf(QStringLiteral("echarts.init(vis,"));
    EXPECT_GT(guardPos, 0);
    EXPECT_GT(initPos, guardPos);
}
