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
#include <QByteArray>

#include "charts/EChartsParser.h"

namespace {

// Payload shape as the StockChartDialog emits it for a non-ECharts engine.
const QByteArray kSharedPayload =
    "{\"title\":\"T\",\"type\":\"line\",\"volume\":true,\"zoom\":true,"
    "\"animate\":false,\"ma\":[5,20],"
    "\"indicators\":{\"ma5\":[null,1.0],\"rsi\":[null,50.0]},"
    "\"dates\":[\"2024-01-02\",\"2024-01-03\"],"
    "\"ohlc\":[[1,2,1,3],[2,3,2,4]],\"volumes\":[100,200]}";

void expectSharedFields(const ChartSource::StockSpecData &out)
{
    EXPECT_EQ(QString::fromUtf8("T"), out.title);
    EXPECT_EQ(ChartSource::StockChartType::Line, out.chartType);
    EXPECT_TRUE(out.volume);
    EXPECT_TRUE(out.zoom);
    EXPECT_FALSE(out.animate);
    EXPECT_TRUE(out.ma5);
    EXPECT_TRUE(out.ma20);
    EXPECT_FALSE(out.ma10);
    EXPECT_FALSE(out.ma50);
    EXPECT_TRUE(out.macd == false && out.rsi && out.boll == false && out.kdj == false);
    ASSERT_EQ(2, out.dates.size());
    EXPECT_EQ(QString::fromUtf8("2024-01-02"), out.dates.at(0));
    ASSERT_EQ(2, out.ohlc.size());
    EXPECT_DOUBLE_EQ(1.0, out.ohlc.at(0).at(0));  // open
    EXPECT_DOUBLE_EQ(2.0, out.ohlc.at(0).at(1));  // close
    EXPECT_DOUBLE_EQ(1.0, out.ohlc.at(0).at(2));  // low
    EXPECT_DOUBLE_EQ(3.0, out.ohlc.at(0).at(3));  // high
    ASSERT_EQ(2, out.volumes.size());
    EXPECT_DOUBLE_EQ(100.0, out.volumes.at(0));
    EXPECT_TRUE(out.hasVolume);
}

} // namespace

TEST(StockParser, DetectLightweight)
{
    const QByteArray body = "scribaStockChart(\"lightweight\", {\"dates\":[]})";
    EXPECT_EQ(ChartSource::StockEngine::Lightweight,
              ChartSource::detectStockEngine(body));
}

TEST(StockParser, DetectKlinecharts)
{
    const QByteArray body = "scribaStockChart(\"klinecharts\", {\"dates\":[]})";
    EXPECT_EQ(ChartSource::StockEngine::KlineCharts,
              ChartSource::detectStockEngine(body));
}

TEST(StockParser, DetectTradex)
{
    const QByteArray body = "scribaStockChart(\"tradex\", {\"dates\":[]})";
    EXPECT_EQ(ChartSource::StockEngine::TradeX,
              ChartSource::detectStockEngine(body));
}

TEST(StockParser, DetectEChartsBySeries)
{
    EXPECT_EQ(ChartSource::StockEngine::ECharts,
              ChartSource::detectStockEngine(
                  "{\"series\":[{\"type\":\"candlestick\"}]}"));
}

TEST(StockParser, UnknownBody)
{
    EXPECT_EQ(ChartSource::StockEngine::Unknown,
              ChartSource::detectStockEngine("garbage"));
    EXPECT_EQ(ChartSource::StockEngine::Unknown,
              ChartSource::detectStockEngine("scribaStockChart(\"bogus\", {})"));
}

TEST(StockParser, ParseLightweightRoundTrip)
{
    const QByteArray body = "scribaStockChart(\"lightweight\", " + kSharedPayload + ")";
    ChartSource::StockSpecData out;
    EXPECT_TRUE(ChartSource::parseLightweightSpec(body, out));
    expectSharedFields(out);
}

TEST(StockParser, ParseKlinechartsRoundTrip)
{
    const QByteArray body = "scribaStockChart(\"klinecharts\", " + kSharedPayload + ")";
    ChartSource::StockSpecData out;
    EXPECT_TRUE(ChartSource::parseKlinechartsSpec(body, out));
    expectSharedFields(out);
}

TEST(StockParser, ParseTradexRoundTrip)
{
    const QByteArray body = "scribaStockChart(\"tradex\", " + kSharedPayload + ")";
    ChartSource::StockSpecData out;
    EXPECT_TRUE(ChartSource::parseTradexSpec(body, out));
    expectSharedFields(out);
}

TEST(StockParser, ParseEngineSpecRejectsWrongEngineWrapper)
{
    ChartSource::StockSpecData out;
    const QByteArray body = "scribaStockChart(\"klinecharts\", " + kSharedPayload + ")";
    EXPECT_FALSE(ChartSource::parseLightweightSpec(body, out));
}

TEST(StockParser, MismatchedArraysRejected)
{
    const QByteArray body = "scribaStockChart(\"lightweight\", "
        "{\"dates\":[\"2024-01-02\",\"2024-01-03\"],"
        "\"ohlc\":[[1,2,1,3]],\"volumes\":[100,200]})";
    ChartSource::StockSpecData out;
    EXPECT_FALSE(ChartSource::parseLightweightSpec(body, out));
}

TEST(StockParser, VolumesWithoutVolumeFlagNotRequired)
{
    const QByteArray body = "scribaStockChart(\"lightweight\", "
        "{\"volume\":false,\"dates\":[\"2024-01-02\"],"
        "\"ohlc\":[[1,2,1,3]],\"volumes\":[]})";
    ChartSource::StockSpecData out;
    EXPECT_TRUE(ChartSource::parseLightweightSpec(body, out));
    EXPECT_FALSE(out.hasVolume);
}

TEST(StockParser, MissingTypeDefaultsToCandlestick)
{
    const QByteArray body = "scribaStockChart(\"lightweight\", "
        "{\"dates\":[\"2024-01-02\"],\"ohlc\":[[1,2,1,3]]})";
    ChartSource::StockSpecData out;
    EXPECT_TRUE(ChartSource::parseLightweightSpec(body, out));
    EXPECT_EQ(ChartSource::StockChartType::Candlestick, out.chartType);
}

TEST(StockParser, EChartsSpecDerivesChartType)
{
    ChartSource::StockSpecData line;
    ASSERT_TRUE(ChartSource::parseStockSpec(
        "{\"series\":[{\"type\":\"line\",\"data\":[[1,2,1,3]]}],"
        "\"xAxis\":{\"data\":[\"2024-01-02\"]}}", line));
    EXPECT_EQ(ChartSource::StockChartType::Line, line.chartType);

    ChartSource::StockSpecData area;
    ASSERT_TRUE(ChartSource::parseStockSpec(
        "{\"series\":[{\"type\":\"line\",\"areaStyle\":{},\"data\":[[1,2,1,3]]}],"
        "\"xAxis\":{\"data\":[\"2024-01-02\"]}}", area));
    EXPECT_EQ(ChartSource::StockChartType::Area, area.chartType);

    ChartSource::StockSpecData bar;
    ASSERT_TRUE(ChartSource::parseStockSpec(
        "{\"series\":[{\"type\":\"bar\",\"data\":[[1,2,1,3]]}],"
        "\"xAxis\":{\"data\":[\"2024-01-02\"]}}", bar));
    EXPECT_EQ(ChartSource::StockChartType::Bar, bar.chartType);
}
