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
#include <QComboBox>
#include <QGroupBox>
#include <QTableWidget>
#include <cmath>
#include "charts/StockChartDialog.h"
#include "charts/Indicators.h"
#include "charts/ChartSource.h"

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

TEST_F(StockChartDialogTest, MovingAverageSemanticsPinnedInIndicatorsSuite) {
    // StockChartDialog::movingAverage was removed in favor of Indicators::sma;
    // the value contract is locked by test_indicators.cpp SmaParityWithOldMovingAverage.
    QList<double> values = {1, 2, 3, 4, 5, 6};
    QList<double> ma3 = Indicators::sma(values, 3);
    ASSERT_EQ(ma3.size(), 6);
    EXPECT_TRUE(std::isnan(ma3[0]));
    EXPECT_TRUE(std::isnan(ma3[1]));
    EXPECT_DOUBLE_EQ(ma3[2], 2.0);
    EXPECT_DOUBLE_EQ(ma3[3], 3.0);
    EXPECT_DOUBLE_EQ(ma3[4], 4.0);
    EXPECT_DOUBLE_EQ(ma3[5], 5.0);
}

TEST_F(StockChartDialogTest, PreviewHtmlDefersInitUntilContainerHasWidth) {
    QString html = StockChartDialog::previewPageHtml("{}", "echarts");
    int guardPos = html.indexOf(QStringLiteral("vis.clientWidth>0"));
    int initPos = html.indexOf(QStringLiteral("echarts.init(vis,"));
    EXPECT_GT(guardPos, 0);
    EXPECT_GT(initPos, guardPos);
}

TEST_F(StockChartDialogTest, EngineSwitchToKlinechartsEmitsKcFence) {
    auto *engine = dlg.findChild<QComboBox *>("stockEngineCombo");
    ASSERT_NE(engine, nullptr);
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::KlineCharts)));
    QString spec = dlg.generatedSpec();
    EXPECT_TRUE(spec.startsWith("\n```kc\n"));
    EXPECT_TRUE(spec.contains(QStringLiteral("scribaStockChart(\"klinecharts\",")));
}

TEST_F(StockChartDialogTest, LightweightFenceCarriesTypeAndPayload) {
    auto *engine = dlg.findChild<QComboBox *>("stockEngineCombo");
    auto *type = dlg.findChild<QComboBox *>("stockTypeCombo");
    ASSERT_NE(engine, nullptr);
    ASSERT_NE(type, nullptr);
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::Lightweight)));
    type->setCurrentIndex(type->findData(
        static_cast<int>(ChartSource::StockChartType::Area)));
    QString spec = dlg.generatedSpec();
    EXPECT_TRUE(spec.startsWith("\n```lc\n"));
    int start = spec.indexOf(QStringLiteral("scribaStockChart("));
    int brace = spec.indexOf('{', start);
    int end = spec.lastIndexOf('}');
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        spec.mid(brace, end - brace + 1).toUtf8(), &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);
    QJsonObject payload = doc.object();
    EXPECT_EQ(payload["type"].toString(), "area");
    EXPECT_TRUE(payload["ohlc"].toArray().size() > 0);
    EXPECT_TRUE(payload["dates"].toArray().size() > 0);
    EXPECT_TRUE(payload["volumes"].toArray().size() > 0);
    EXPECT_TRUE(payload["ma"].toArray().contains(5));
    EXPECT_TRUE(payload["ma"].toArray().contains(20));
    EXPECT_TRUE(payload.contains("zoom"));
}

TEST_F(StockChartDialogTest, IndicatorCheckboxesEmitComputedSeries) {
    auto *engine = dlg.findChild<QComboBox *>("stockEngineCombo");
    ASSERT_NE(engine, nullptr);
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::KlineCharts)));

    for (QCheckBox *check : dlg.findChildren<QCheckBox *>()) {
        if (check->text() == "RSI") check->setChecked(true);
        if (check->text() == "MACD") check->setChecked(true);
    }

    QString spec = dlg.generatedSpec();
    int brace = spec.indexOf('{');
    int end = spec.lastIndexOf('}');
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        spec.mid(brace, end - brace + 1).toUtf8(), &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);
    QJsonObject payload = doc.object();
    QJsonObject indicators = payload["indicators"].toObject();
    ASSERT_FALSE(indicators.isEmpty());

    QJsonArray closes;
    for (const QJsonValue &v : payload["ohlc"].toArray())
        closes.append(v.toArray()[1].toDouble());
    ASSERT_TRUE(closes.size() > 0);

    QJsonArray rsi = indicators["rsi"].toArray();
    ASSERT_EQ(rsi.size(), closes.size());
    for (int i = 0; i < rsi.size(); ++i) {
        if (i < 14)
            EXPECT_TRUE(rsi[i].isNull()) << "rsi[" << i << "] must be null";
        else
            EXPECT_FALSE(rsi[i].isNull()) << "rsi[" << i << "] must be a value";
    }

    QJsonObject macd = indicators["macd"].toObject();
    EXPECT_TRUE(macd.contains("diff"));
    EXPECT_TRUE(macd.contains("dea"));
    EXPECT_TRUE(macd.contains("hist"));
    EXPECT_EQ(macd["diff"].toArray().size(), closes.size());
}

TEST_F(StockChartDialogTest, EChartsEngineDisablesIndicatorGroup) {
    auto *engine = dlg.findChild<QComboBox *>("stockEngineCombo");
    auto *group = dlg.findChild<QGroupBox *>("stockIndicatorGroup");
    ASSERT_NE(engine, nullptr);
    ASSERT_NE(group, nullptr);
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::KlineCharts)));
    EXPECT_TRUE(group->isEnabled());
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::ECharts)));
    EXPECT_FALSE(group->isEnabled());
    for (QCheckBox *check : group->findChildren<QCheckBox *>())
        EXPECT_FALSE(check->isChecked());
}

TEST_F(StockChartDialogTest, PrefillFromKcLineRestoresEngineTypeAndIndicators) {
    auto *engine = dlg.findChild<QComboBox *>("stockEngineCombo");
    auto *type = dlg.findChild<QComboBox *>("stockTypeCombo");
    auto *table = dlg.findChild<QTableWidget *>();
    ASSERT_NE(engine, nullptr);
    ASSERT_NE(type, nullptr);
    ASSERT_NE(table, nullptr);
    int originalRows = table->rowCount();
    engine->setCurrentIndex(engine->findData(
        static_cast<int>(ChartSource::StockEngine::KlineCharts)));
    type->setCurrentIndex(type->findData(
        static_cast<int>(ChartSource::StockChartType::Line)));
    for (QCheckBox *check : dlg.findChildren<QCheckBox *>())
        if (check->text() == "RSI") check->setChecked(true);
    QString generated = dlg.generatedSpec();
    int fence = generated.indexOf(QStringLiteral("```kc\n")) + 5;
    QString body = generated.mid(fence,
        generated.indexOf(QStringLiteral("\n```"), fence) - fence);

    StockChartDialog reopened(body);
    auto *rEngine = reopened.findChild<QComboBox *>("stockEngineCombo");
    auto *rType = reopened.findChild<QComboBox *>("stockTypeCombo");
    auto *rTable = reopened.findChild<QTableWidget *>();
    ASSERT_NE(rEngine, nullptr);
    ASSERT_NE(rType, nullptr);
    ASSERT_NE(rTable, nullptr);
    EXPECT_EQ(static_cast<ChartSource::StockEngine>(rEngine->currentData().toInt()),
              ChartSource::StockEngine::KlineCharts);
    EXPECT_EQ(static_cast<ChartSource::StockChartType>(rType->currentData().toInt()),
              ChartSource::StockChartType::Line);
    EXPECT_EQ(rTable->rowCount(), originalRows);
    bool rsi = false;
    for (QCheckBox *check : reopened.findChildren<QCheckBox *>())
        if (check->text() == "RSI") rsi = check->isChecked();
    EXPECT_TRUE(rsi);
}

TEST_F(StockChartDialogTest, TypeChangeDrivesEChartsSeriesType) {
    auto *type = dlg.findChild<QComboBox *>("stockTypeCombo");
    ASSERT_NE(type, nullptr);
    type->setCurrentIndex(type->findData(
        static_cast<int>(ChartSource::StockChartType::Line)));
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray series = obj["series"].toArray();
    ASSERT_FALSE(series.isEmpty());
    EXPECT_EQ(series[0].toObject()["type"].toString(), "line");
    QJsonArray data = series[0].toObject()["data"].toArray();
    ASSERT_FALSE(data.isEmpty());
    EXPECT_EQ(data[0].type(), QJsonValue::Double) << "line series carries closes only";
    EXPECT_DOUBLE_EQ(data[0].toDouble(), 153.9);
}
