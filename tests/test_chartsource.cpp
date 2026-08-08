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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ChartSource.h"

namespace {

QByteArray json(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

// ---------------------------------------------------------------------------
// EcType detection
// ---------------------------------------------------------------------------

TEST(ChartSourceDetect, StockCandlestick) {
    QJsonObject spec;
    QJsonArray series;
    QJsonObject s;
    s["type"] = "candlestick";
    s["data"] = QJsonArray{QJsonArray{1.0, 2.0, 0.5, 3.0}};
    series.append(s);
    spec["series"] = series;
    spec["grid"] = QJsonArray{ QJsonObject{}, QJsonObject{} };
    EXPECT_EQ(ChartSource::detectEcType(json(spec)), ChartSource::EcType::Stock);
}

TEST(ChartSourceDetect, ChartBuilderSeries) {
    for (const char *type : {"bar", "line", "scatter", "pie", "funnel", "gauge",
                             "radar", "heatmap"}) {
        QJsonObject spec;
        QJsonArray series;
        QJsonObject s;
        s["type"] = type;
        s["data"] = QJsonArray{1, 2, 3};
        series.append(s);
        spec["series"] = series;
        EXPECT_EQ(ChartSource::detectEcType(json(spec)), ChartSource::EcType::Chart)
            << type;
    }
}

TEST(ChartSourceDetect, UnknownSpec) {
    EXPECT_EQ(ChartSource::detectEcType("not json"), ChartSource::EcType::Unknown);
    EXPECT_EQ(ChartSource::detectEcType("{}"), ChartSource::EcType::Unknown);
    EXPECT_EQ(ChartSource::detectEcType("[]"), ChartSource::EcType::Unknown);
}

// ---------------------------------------------------------------------------
// Chart Builder spec round-trip (mirrors ChartDialog::buildSpec)
// ---------------------------------------------------------------------------

static QJsonObject chartBuilderSpec(const char *type, bool animate, bool tooltip,
                                    const QString &title,
                                    const QStringList &xValues,
                                    const QList<double> &yValues, bool numericX)
{
    QJsonObject spec;
    if (!animate)
        spec["animation"] = false;
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }
    if (tooltip) {
        QJsonObject tt;
        tt["trigger"] = "axis";
        spec["tooltip"] = tt;
    }
    QJsonObject s;
    s["type"] = type;
    if (qstrcmp(type, "pie") == 0) {
        QJsonArray data;
        for (int i = 0; i < xValues.size(); ++i) {
            QJsonObject item;
            item["name"] = xValues[i];
            item["value"] = yValues[i];
            data.append(item);
        }
        s["data"] = data;
    } else if (numericX) {
        QJsonArray pairs;
        for (int i = 0; i < xValues.size(); ++i) {
            QJsonArray pair{xValues[i].toDouble(), yValues[i]};
            pairs.append(pair);
        }
        s["data"] = pairs;
    } else {
        QJsonArray cats, vals;
        for (int i = 0; i < xValues.size(); ++i) {
            cats.append(xValues[i]);
            vals.append(yValues[i]);
        }
        QJsonObject xAxis;
        xAxis["type"] = "category";
        xAxis["data"] = cats;
        spec["xAxis"] = xAxis;
        QJsonObject yAxis;
        yAxis["type"] = "value";
        spec["yAxis"] = yAxis;
        s["data"] = vals;
    }
    spec["series"] = QJsonArray{s};
    return spec;
}

TEST(ChartSourceChart, PieRoundTrip) {
    QJsonObject spec = chartBuilderSpec("pie", true, false, "My Pie",
        {"Alpha", "Beta"}, {30, 70}, false);
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "pie");
    EXPECT_EQ(out.title, "My Pie");
    EXPECT_TRUE(out.animate);
    EXPECT_FALSE(out.tooltip);
    EXPECT_EQ(out.headers, (QStringList{"Label", "Value"}));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"Alpha", "30"}));
    EXPECT_EQ(out.rows[1], (QStringList{"Beta", "70"}));
}

TEST(ChartSourceChart, CategoryBarRoundTrip) {
    QJsonObject spec = chartBuilderSpec("bar", false, true, QString(),
        {"Q1", "Q2", "Q3"}, {10, 20, 30}, false);
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "bar");
    EXPECT_TRUE(out.title.isEmpty());
    EXPECT_FALSE(out.animate);
    EXPECT_TRUE(out.tooltip);
    EXPECT_EQ(out.headers, (QStringList{"Category", "Value"}));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[2], (QStringList{"Q3", "30"}));
}

TEST(ChartSourceChart, NumericScatterRoundTrip) {
    QJsonObject spec = chartBuilderSpec("scatter", true, false, "scatter",
        {"1", "2", "3"}, {1.5, 2.5, 3.5}, true);
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "scatter");
    EXPECT_EQ(out.headers, (QStringList{"X", "Y"}));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[1], (QStringList{"2", "2.5"}));
}

TEST(ChartSourceChart, AreaDetectedFromLine) {
    QJsonObject spec = chartBuilderSpec("line", true, false, "a",
        {"A", "B"}, {1, 2}, false);
    QJsonArray series = spec.value("series").toArray();
    QJsonObject s = series.at(0).toObject();
    s["areaStyle"] = QJsonObject();
    series.replace(0, s);
    spec["series"] = series;
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "area");
}

TEST(ChartSourceChart, CandlestickRejected) {
    QJsonObject spec = chartBuilderSpec("bar", true, false, "x",
        {"A"}, {1}, false);
    QJsonArray series = spec.value("series").toArray();
    QJsonObject s = series.at(0).toObject();
    s["type"] = "candlestick";
    series.replace(0, s);
    spec["series"] = series;
    ChartSource::ChartSpecData out;
    EXPECT_FALSE(ChartSource::parseChartSpec(json(spec), out));
}

// ---------------------------------------------------------------------------
// New chart-builder types (funnel/gauge/radar/heatmap/calendar) — mirror
// ChartDialog::buildSpec
// ---------------------------------------------------------------------------

// Name/value item charts (funnel, gauge) — no axes.
static QJsonObject itemChartSpec(const char *type, const QString &title,
                                 const QStringList &names, const QList<double> &values)
{
    QJsonObject spec;
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }
    QJsonArray items;
    for (int i = 0; i < names.size(); ++i) {
        QJsonObject item;
        item["name"] = names[i];
        item["value"] = values[i];
        items.append(item);
    }
    QJsonObject s;
    s["type"] = type;
    if (qstrcmp(type, "gauge") == 0) {
        s["min"] = 0;
        s["max"] = values.isEmpty() ? 100 : values.last();
    }
    s["data"] = items;
    spec["series"] = QJsonArray{s};
    return spec;
}

TEST(ChartSourceChart, FunnelRoundTrip) {
    QJsonObject spec = itemChartSpec("funnel", "Conversion",
        {"Visited", "Retained"}, {100.0, 60.0});
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "funnel");
    EXPECT_EQ(out.title, "Conversion");
    EXPECT_EQ(out.headers, (QStringList{"Label", "Value"}));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"Visited", "100"}));
    EXPECT_EQ(out.rows[1], (QStringList{"Retained", "60"}));
}

TEST(ChartSourceChart, GaugeRoundTrip) {
    QJsonObject spec = itemChartSpec("gauge", {}, {"Speed"}, {168.0});
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "gauge");
    EXPECT_EQ(out.headers, (QStringList{"Label", "Value"}));
    ASSERT_EQ(out.rows.size(), 1);
    EXPECT_EQ(out.rows[0], (QStringList{"Speed", "168"}));
}

TEST(ChartSourceChart, RadarRoundTrip) {
    QJsonObject spec;
    QJsonArray indicators;
    QJsonArray values;
    const QStringList names = {"Sales", "Marketing", "Dev"};
    const QList<double> nums = {4200.0, 25000.0, 30000.0};
    const QList<double> maxs = {6500.0, 25000.0, 52000.0};
    for (int i = 0; i < names.size(); ++i) {
        QJsonObject ind;
        ind["name"] = names[i];
        ind["max"] = maxs[i];
        indicators.append(ind);
        values.append(nums[i]);
    }
    QJsonObject radar;
    radar["indicator"] = indicators;
    spec["radar"] = radar;
    QJsonObject s;
    s["type"] = "radar";
    QJsonObject dataItem;
    dataItem["value"] = values;
    s["data"] = QJsonArray{dataItem};
    spec["series"] = QJsonArray{s};

    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "radar");
    EXPECT_EQ(out.headers, (QStringList{"Indicator", "Value", "Max"}));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[0], (QStringList{"Sales", "4200", "6500"}));
    EXPECT_EQ(out.rows[1], (QStringList{"Marketing", "25000", "25000"}));
}

TEST(ChartSourceChart, CalendarHeatmapRoundTrip) {
    QJsonObject s;
    s["type"] = "heatmap";
    s["coordinateSystem"] = "calendar";
    s["data"] = QJsonArray{QJsonArray{"2026-07-01", 1.0},
                           QJsonArray{"2026-07-02", 4.0}};
    QJsonObject spec;
    spec["series"] = QJsonArray{s};
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "calendar");
    EXPECT_EQ(out.headers, (QStringList{"Date", "Value"}));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"2026-07-01", "1"}));
    EXPECT_EQ(out.rows[1], (QStringList{"2026-07-02", "4"}));
}

TEST(ChartSourceChart, MatrixHeatmapRoundTrip) {
    QJsonObject spec;
    QJsonObject xAxis;
    xAxis["type"] = "category";
    xAxis["data"] = QJsonArray{"Mon", "Tue", "Wed"};
    QJsonObject yAxis;
    yAxis["type"] = "category";
    yAxis["data"] = QJsonArray{"Morning", "Evening"};
    spec["xAxis"] = xAxis;
    spec["yAxis"] = yAxis;
    QJsonObject s;
    s["type"] = "heatmap";
    s["data"] = QJsonArray{QJsonArray{0, 0, 5.0},
                           QJsonArray{1, 0, 7.0},
                           QJsonArray{2, 1, 3.0}};
    spec["series"] = QJsonArray{s};

    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "heatmap");
    EXPECT_EQ(out.headers, (QStringList{"X", "Y", "Value"}));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[0], (QStringList{"Mon", "Morning", "5"}));
    EXPECT_EQ(out.rows[1], (QStringList{"Tue", "Morning", "7"}));
    EXPECT_EQ(out.rows[2], (QStringList{"Wed", "Evening", "3"}));
}

TEST(ChartSourceChart, MatrixHeatmapMissingAxesRejected) {
    QJsonObject s;
    s["type"] = "heatmap";
    s["data"] = QJsonArray{QJsonArray{0, 0, 5.0}};
    QJsonObject spec;
    spec["series"] = QJsonArray{s};
    ChartSource::ChartSpecData out;
    EXPECT_FALSE(ChartSource::parseChartSpec(json(spec), out));
}

TEST(ChartSourceChart, RadarMismatchedIndicatorCountRejected) {
    QJsonObject radar;
    radar["indicator"] = QJsonArray{QJsonObject{{"name", "A"}, {"max", 10}}};
    QJsonObject s;
    s["type"] = "radar";
    s["data"] = QJsonArray{QJsonObject{{"value", QJsonArray{1.0, 2.0}}}};
    QJsonObject spec;
    spec["radar"] = radar;
    spec["series"] = QJsonArray{s};
    ChartSource::ChartSpecData out;
    EXPECT_FALSE(ChartSource::parseChartSpec(json(spec), out));
}

// ---------------------------------------------------------------------------
// Stock spec round-trip (mirrors StockChartDialog::buildSpec)
// ---------------------------------------------------------------------------

static QJsonObject stockSpec(bool animate, const QString &title, bool volume,
                             bool zoom, bool ma5, bool ma20)
{
    QJsonObject spec;
    if (!animate)
        spec["animation"] = false;
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }
    QJsonObject tooltip;
    tooltip["trigger"] = "axis";
    spec["tooltip"] = tooltip;

    QJsonArray dates{"2026-06-01", "2026-06-02", "2026-06-03"};
    QJsonArray ohlc;
    ohlc.append(QJsonArray{10.0, 11.0, 9.0, 12.0}); // open, close, low, high
    ohlc.append(QJsonArray{11.0, 10.5, 10.0, 11.5});
    ohlc.append(QJsonArray{10.5, 12.0, 10.2, 12.5});
    QJsonObject candle;
    candle["name"] = "OHLC";
    candle["type"] = "candlestick";
    candle["data"] = ohlc;
    QJsonArray series;
    series.append(candle);
    if (ma5) {
        QJsonObject ma;
        ma["name"] = "MA5";
        ma["type"] = "line";
        ma["data"] = QJsonArray{QJsonValue::Null, 10.5, 11.0};
        series.append(ma);
    }
    if (ma20) {
        QJsonObject ma;
        ma["name"] = "MA20";
        ma["type"] = "line";
        ma["data"] = QJsonArray{10.0, 10.5, 11.0};
        series.append(ma);
    }
    if (volume) {
        QJsonObject v;
        v["name"] = "Volume";
        v["type"] = "bar";
        v["xAxisIndex"] = 1;
        v["yAxisIndex"] = 1;
        v["data"] = QJsonArray{100, 200, QJsonValue::Null};
        series.append(v);
        QJsonArray grid;
        grid.append(QJsonObject{});
        grid.append(QJsonObject{});
        spec["grid"] = grid;
    } else {
        spec["grid"] = QJsonArray{QJsonObject{}};
    }
    spec["series"] = series;
    QJsonObject xAxis0;
    xAxis0["data"] = dates;
    spec["xAxis"] = QJsonArray{xAxis0};
    spec["yAxis"] = QJsonArray{QJsonObject{}};
    if (zoom) {
        spec["dataZoom"] = QJsonArray{QJsonObject{}};
    }
    return spec;
}

TEST(ChartSourceStock, RoundTrip) {
    QJsonObject spec = stockSpec(true, "ACME", true, true, true, true);
    ChartSource::StockSpecData out;
    ASSERT_TRUE(ChartSource::parseStockSpec(json(spec), out));
    EXPECT_EQ(out.title, "ACME");
    EXPECT_TRUE(out.volume);
    EXPECT_TRUE(out.zoom);
    EXPECT_TRUE(out.animate);
    EXPECT_TRUE(out.ma5);
    EXPECT_TRUE(out.ma20);
    EXPECT_FALSE(out.ma10);
    EXPECT_FALSE(out.ma50);
    EXPECT_TRUE(out.hasVolume);
    ASSERT_EQ(out.dates.size(), 3);
    EXPECT_EQ(out.dates[0], "2026-06-01");
    ASSERT_EQ(out.ohlc.size(), 3);
    // [open, close, low, high]
    EXPECT_DOUBLE_EQ(out.ohlc[0][0], 10.0);
    EXPECT_DOUBLE_EQ(out.ohlc[0][1], 11.0);
    EXPECT_DOUBLE_EQ(out.ohlc[0][2], 9.0);
    EXPECT_DOUBLE_EQ(out.ohlc[0][3], 12.0);
    ASSERT_EQ(out.volumes.size(), 3);
    EXPECT_DOUBLE_EQ(out.volumes[0], 100);
    EXPECT_DOUBLE_EQ(out.volumes[2], 0); // null -> 0
}

TEST(ChartSourceStock, NoVolumeNoZoom) {
    QJsonObject spec = stockSpec(false, QString(), false, false, false, false);
    ChartSource::StockSpecData out;
    ASSERT_TRUE(ChartSource::parseStockSpec(json(spec), out));
    EXPECT_TRUE(out.title.isEmpty());
    EXPECT_FALSE(out.volume);
    EXPECT_FALSE(out.zoom);
    EXPECT_FALSE(out.animate);
    EXPECT_FALSE(out.hasVolume);
    EXPECT_TRUE(out.volumes.isEmpty());
}

// ---------------------------------------------------------------------------
// Folded cartesian types: effectScatter + pictorialBar
// ---------------------------------------------------------------------------

TEST(ChartSourceChart, EffectScatterDetectedAsChart) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "effectScatter";
    s["data"] = QJsonArray{QJsonArray{1.0, 2.0}, QJsonArray{3.0, 4.0}};
    spec["series"] = QJsonArray{s};
    EXPECT_EQ(ChartSource::detectEcType(json(spec)), ChartSource::EcType::Chart);
}

TEST(ChartSourceChart, EffectScatterRoundTrip) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "effectScatter";
    s["symbolSize"] = 28;
    s["rippleEffect"] = QJsonObject{{"scale", 4}};
    s["data"] = QJsonArray{QJsonArray{10.0, 20.0}, QJsonArray{30.0, 45.0}};
    spec["series"] = QJsonArray{s};

    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "effectScatter");
    EXPECT_TRUE(out.rippleEffect);
    EXPECT_EQ(out.headers, (QStringList{"X", "Y"}));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"10", "20"}));
    EXPECT_EQ(out.rows[1], (QStringList{"30", "45"}));
}

TEST(ChartSourceChart, EffectScatterNamedValueItems) {
    // Docs example wraps points in {value:[x,y], name:"..."} objects.
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "effectScatter";
    s["rippleEffect"] = QJsonObject{{"scale", 4}};
    s["data"] = QJsonArray{
        QJsonObject{{"value", QJsonArray{10.0, 20.0}}, {"name", "Site A"}},
        QJsonObject{{"value", QJsonArray{30.0, 45.0}}, {"name", "Site B"}},
    };
    spec["series"] = QJsonArray{s};

    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.headers, (QStringList{"X", "Y"}));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"10", "20"}));
}

TEST(ChartSourceChart, EffectScatterWithoutRippleIsFalse) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "effectScatter";
    s["data"] = QJsonArray{QJsonArray{1.0, 2.0}};
    spec["series"] = QJsonArray{s};
    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_FALSE(out.rippleEffect);
}

TEST(ChartSourceChart, PictorialBarDetectedAsChart) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "pictorialBar";
    s["data"] = QJsonArray{60, 80, 50};
    spec["series"] = QJsonArray{s};
    EXPECT_EQ(ChartSource::detectEcType(json(spec)), ChartSource::EcType::Chart);
}

TEST(ChartSourceChart, PictorialBarRoundTrip) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "pictorialBar";
    s["symbol"] = "rect";
    s["symbolRepeat"] = true;
    s["symbolSize"] = QJsonArray{12, 16};
    s["data"] = QJsonArray{60.0, 80.0, 50.0};
    QJsonObject xAxis;
    xAxis["type"] = "category";
    xAxis["data"] = QJsonArray{"Mon", "Tue", "Wed"};
    spec["xAxis"] = xAxis;
    spec["series"] = QJsonArray{s};

    ChartSource::ChartSpecData out;
    ASSERT_TRUE(ChartSource::parseChartSpec(json(spec), out));
    EXPECT_EQ(out.type, "pictorialBar");
    EXPECT_TRUE(out.repeatSymbol);
    EXPECT_EQ(out.headers, (QStringList{"Category", "Value"}));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[0], (QStringList{"Mon", "60"}));
}

// ---------------------------------------------------------------------------
// Advanced Charts types (sankey / boxplot / parallel / themeRiver / graph /
// treemap / sunburst) — JSON mirrors of AdvancedChartDialog::build*
// ---------------------------------------------------------------------------

TEST(ChartSourceDetect, AdvancedTypes) {
    struct Pair { const char *type; ChartSource::EcType expected; };
    const Pair pairs[] = {
        {"sankey",   ChartSource::EcType::Sankey},
        {"boxplot",  ChartSource::EcType::Boxplot},
        {"parallel", ChartSource::EcType::Parallel},
        {"themeRiver", ChartSource::EcType::ThemeRiver},
        {"graph",    ChartSource::EcType::Graph},
        {"treemap",  ChartSource::EcType::Treemap},
        {"sunburst", ChartSource::EcType::Sunburst},
    };
    for (const Pair &p : pairs) {
        QJsonObject spec;
        QJsonObject s;
        s["type"] = p.type;
        s["data"] = QJsonArray{};
        spec["series"] = QJsonArray{s};
        EXPECT_EQ(ChartSource::detectEcType(json(spec)), p.expected) << p.type;
    }
}

TEST(ChartSourceSankey, RoundTrip) {
    QJsonObject spec;
    QJsonArray data;
    for (const char *n : {"a", "b", "a1", "b1", "c", "e"})
        data.append(QJsonObject{{"name", n}});
    QJsonObject s;
    s["type"] = "sankey";
    s["data"] = data;
    s["links"] = QJsonArray{
        QJsonObject{{"source", "a"}, {"target", "a1"}, {"value", 5}},
        QJsonObject{{"source", "e"}, {"target", "b"}, {"value", 3}},
        QJsonObject{{"source", "b1"}, {"target", "c"}, {"value", 2}},
    };
    spec["series"] = QJsonArray{s};

    ChartSource::SankeySpecData out;
    ASSERT_TRUE(ChartSource::parseSankeySpec(json(spec), out));
    EXPECT_TRUE(out.title.isEmpty());
    ASSERT_EQ(out.links.size(), 3);
    EXPECT_EQ(out.links[0], (QStringList{"a", "a1", "5"}));
    EXPECT_EQ(out.links[1], (QStringList{"e", "b", "3"}));
    EXPECT_EQ(out.links[2], (QStringList{"b1", "c", "2"}));
}

TEST(ChartSourceBoxplot, RoundTrip) {
    QJsonObject spec;
    QJsonObject xAxis;
    xAxis["type"] = "category";
    xAxis["data"] = QJsonArray{"Class A", "Class B", "Class C"};
    QJsonObject s;
    s["type"] = "boxplot";
    s["data"] = QJsonArray{QJsonArray{40, 56, 72, 88, 96},
                           QJsonArray{32, 51, 68, 84, 95},
                           QJsonArray{28, 46, 62, 80, 91}};
    spec["xAxis"] = xAxis;
    spec["series"] = QJsonArray{s};

    ChartSource::BoxplotSpecData out;
    ASSERT_TRUE(ChartSource::parseBoxplotSpec(json(spec), out));
    EXPECT_EQ(out.categories, (QStringList{"Class A", "Class B", "Class C"}));
    ASSERT_EQ(out.stats.size(), 3);
    ASSERT_EQ(out.stats[0].size(), 5);
    EXPECT_DOUBLE_EQ(out.stats[0][0], 40);
    EXPECT_DOUBLE_EQ(out.stats[0][4], 96);
    EXPECT_DOUBLE_EQ(out.stats[2][2], 62);
}

TEST(ChartSourceBoxplot, MismatchedCountsRejected) {
    QJsonObject spec;
    QJsonObject xAxis;
    xAxis["data"] = QJsonArray{"A", "B"};
    QJsonObject s;
    s["type"] = "boxplot";
    s["data"] = QJsonArray{QJsonArray{1, 2, 3, 4, 5}};
    spec["xAxis"] = xAxis;
    spec["series"] = QJsonArray{s};
    ChartSource::BoxplotSpecData out;
    EXPECT_FALSE(ChartSource::parseBoxplotSpec(json(spec), out));
}

TEST(ChartSourceParallel, RoundTrip) {
    QJsonObject spec;
    spec["parallelAxis"] = QJsonArray{
        QJsonObject{{"dim", 0}, {"name", "Dim 0"}},
        QJsonObject{{"dim", 1}, {"name", "Dim 1"}},
        QJsonObject{{"dim", 2}, {"name", "Dim 2"}},
        QJsonObject{{"dim", 3}, {"name", "Dim 3"}},
    };
    QJsonObject s;
    s["type"] = "parallel";
    s["data"] = QJsonArray{QJsonArray{1, 3, 2, 4}, QJsonArray{2, 4, 1, 3}};
    spec["series"] = QJsonArray{s};

    ChartSource::ParallelSpecData out;
    ASSERT_TRUE(ChartSource::parseParallelSpec(json(spec), out));
    EXPECT_EQ(out.dimensions,
              (QStringList{"Dim 0", "Dim 1", "Dim 2", "Dim 3"}));
    ASSERT_EQ(out.lines.size(), 2);
    ASSERT_EQ(out.lines[0].size(), 4);
    EXPECT_DOUBLE_EQ(out.lines[0][3], 4);
    EXPECT_DOUBLE_EQ(out.lines[1][1], 4);
}

TEST(ChartSourceParallel, SparseDimArray) {
    // parallelAxis may omit dim indices; the parser must re-sort by dim.
    QJsonObject spec;
    spec["parallelAxis"] = QJsonArray{
        QJsonObject{{"dim", 2}, {"name", "Z"}},
        QJsonObject{{"dim", 0}, {"name", "A"}},
        QJsonObject{{"dim", 1}, {"name", "M"}},
    };
    QJsonObject s;
    s["type"] = "parallel";
    s["data"] = QJsonArray{QJsonArray{1, 2, 3}};
    spec["series"] = QJsonArray{s};
    ChartSource::ParallelSpecData out;
    ASSERT_TRUE(ChartSource::parseParallelSpec(json(spec), out));
    EXPECT_EQ(out.dimensions, (QStringList{"A", "M", "Z"}));
    EXPECT_DOUBLE_EQ(out.lines[0][2], 3);
}

TEST(ChartSourceThemeRiver, RoundTrip) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "themeRiver";
    s["data"] = QJsonArray{QJsonArray{"2026-01-01", 5, "Apple"},
                           QJsonArray{"2026-01-02", 6, "Banana"}};
    spec["series"] = QJsonArray{s};

    ChartSource::ThemeRiverSpecData out;
    ASSERT_TRUE(ChartSource::parseThemeRiverSpec(json(spec), out));
    ASSERT_EQ(out.rows.size(), 2);
    EXPECT_EQ(out.rows[0], (QStringList{"2026-01-01", "5", "Apple"}));
    EXPECT_EQ(out.rows[1], (QStringList{"2026-01-02", "6", "Banana"}));
}

TEST(ChartSourceGraph, RoundTrip) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "graph";
    s["data"] = QJsonArray{
        QJsonObject{{"name", "N1"}, {"value", 10}},
        QJsonObject{{"name", "N2"}, {"value", 20}},
    };
    s["links"] = QJsonArray{
        QJsonObject{{"source", "N1"}, {"target", "N2"}},
    };
    spec["series"] = QJsonArray{s};

    ChartSource::GraphSpecData out;
    ASSERT_TRUE(ChartSource::parseGraphSpec(json(spec), out));
    EXPECT_EQ(out.nodeNames, (QStringList{"N1", "N2"}));
    ASSERT_EQ(out.nodeValues.size(), 2);
    EXPECT_DOUBLE_EQ(out.nodeValues[0], 10);
    ASSERT_EQ(out.links.size(), 1);
    EXPECT_EQ(out.links[0], (QStringList{"N1", "N2", "0"}));
}

TEST(ChartSourceGraph, MissingNodesRejected) {
    QJsonObject spec;
    QJsonObject s;
    s["type"] = "graph";
    s["links"] = QJsonArray{QJsonObject{{"source", "N1"}, {"target", "N2"}}};
    spec["series"] = QJsonArray{s};
    ChartSource::GraphSpecData out;
    EXPECT_FALSE(ChartSource::parseGraphSpec(json(spec), out));
}

TEST(ChartSourceTree, TreemapLeafFlattening) {
    QJsonObject spec;
    QJsonObject a;
    a["name"] = "nodeA";
    a["value"] = 10;
    a["children"] = QJsonArray{
        QJsonObject{{"name", "nodeAa"}, {"value", 4}},
        QJsonObject{{"name", "nodeAb"}, {"value", 6}},
    };
    QJsonObject b;
    b["name"] = "nodeB";
    b["value"] = 20;
    b["children"] = QJsonArray{
        QJsonObject{{"name", "nodeBa"}, {"value", 12}},
        QJsonObject{{"name", "nodeBb"}, {"value", 8}},
    };
    QJsonObject s;
    s["type"] = "treemap";
    s["data"] = QJsonArray{a, b};
    spec["series"] = QJsonArray{s};

    ChartSource::TreeSpecData out;
    ASSERT_TRUE(ChartSource::parseTreeSpec(json(spec), out));
    ASSERT_EQ(out.rows.size(), 4);
    EXPECT_EQ(out.rows[0], (QStringList{"nodeA", "nodeAa", "4"}));
    EXPECT_EQ(out.rows[1], (QStringList{"nodeA", "nodeAb", "6"}));
    EXPECT_EQ(out.rows[2], (QStringList{"nodeB", "nodeBa", "12"}));
    EXPECT_EQ(out.rows[3], (QStringList{"nodeB", "nodeBb", "8"}));
}

TEST(ChartSourceTree, SunburstLeafFlatten) {
    // Sunburst data is usually a single root whose children carry their own
    // children — the parser must treat the outer array as the root set.
    QJsonObject root;
    root["name"] = "nodeA";
    root["value"] = 10;
    root["children"] = QJsonArray{
        QJsonObject{{"name", "nodeAa"}, {"value", 4}},
        QJsonObject{{"name", "nodeAb"},
                    {"children", QJsonArray{
                        QJsonObject{{"name", "nodeAb1"}, {"value", 3}},
                        QJsonObject{{"name", "nodeAb2"}, {"value", 3}}}}},
    };
    QJsonObject s;
    s["type"] = "sunburst";
    s["data"] = QJsonArray{root};
    QJsonObject spec;
    spec["series"] = QJsonArray{s};

    ChartSource::TreeSpecData out;
    ASSERT_TRUE(ChartSource::parseTreeSpec(json(spec), out));
    ASSERT_EQ(out.rows.size(), 3);
    EXPECT_EQ(out.rows[0], (QStringList{"nodeA", "nodeAa", "4"}));
    EXPECT_EQ(out.rows[1], (QStringList{"nodeA", "nodeAb", "nodeAb1", "3"}));
    EXPECT_EQ(out.rows[2], (QStringList{"nodeA", "nodeAb", "nodeAb2", "3"}));
}

// ---------------------------------------------------------------------------
// Mermaid
// ---------------------------------------------------------------------------

TEST(ChartSourceMermaid, DetectType) {
    EXPECT_EQ(ChartSource::detectMermaidType("pie title X"), ChartSource::MermaidType::Pie);
    EXPECT_EQ(ChartSource::detectMermaidType("flowchart TD\n  A-->B"),
              ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(ChartSource::detectMermaidType("sequenceDiagram"),
              ChartSource::MermaidType::Sequence);
    EXPECT_EQ(ChartSource::detectMermaidType("gantt"), ChartSource::MermaidType::Gantt);
    EXPECT_EQ(ChartSource::detectMermaidType("classDiagram"), ChartSource::MermaidType::Class);
    EXPECT_EQ(ChartSource::detectMermaidType("erDiagram"), ChartSource::MermaidType::ER);
    EXPECT_EQ(ChartSource::detectMermaidType("stateDiagram-v2"),
              ChartSource::MermaidType::State);
    EXPECT_EQ(ChartSource::detectMermaidType("mindmap"), ChartSource::MermaidType::Mindmap);
    EXPECT_EQ(ChartSource::detectMermaidType("timeline"), ChartSource::MermaidType::Timeline);
    EXPECT_EQ(ChartSource::detectMermaidType("journey"), ChartSource::MermaidType::Journey);
    EXPECT_EQ(ChartSource::detectMermaidType("quadrantChart"),
              ChartSource::MermaidType::Quadrant);
    EXPECT_EQ(ChartSource::detectMermaidType("sankey-beta"), ChartSource::MermaidType::Sankey);
    EXPECT_EQ(ChartSource::detectMermaidType("random"), ChartSource::MermaidType::Unknown);
}

TEST(ChartSourceMermaid, DetectSkipsCommentsAndDirectives) {
    // Real-world diagrams frequently open with `%%{init: {...}}%%` config
    // blocks or `%%` comments; detection must not mistake them for Unknown.
    EXPECT_EQ(ChartSource::detectMermaidType(
                  "%%{init: {\"theme\": \"base\"}}%%\nflowchart LR\n  A --> B\n"),
              ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(ChartSource::detectMermaidType(
                  "%% a comment\npie title Usage\n  \"A\" : 1\n"),
              ChartSource::MermaidType::Pie);
    ChartSource::MermaidData out;
    EXPECT_TRUE(ChartSource::parseMermaid(
        "%%{init: {\"theme\": \"base\"}}%%\nflowchart LR\n  A --> B\n", out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(out.fcNodes.size(), 2);
    EXPECT_EQ(out.fcEdges.size(), 1);
}

// The following inputs mirror MermaidDialog::build*Diagram output byte-for-byte.

TEST(ChartSourceMermaid, Pie) {
    const QString diagram =
        "pie title My Pie Chart\n"
        "    \"Alpha\" : 30\n"
        "    \"Beta\" : 70\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Pie);
    EXPECT_EQ(out.pieTitle, "My Pie Chart");
    ASSERT_EQ(out.pieEntries.size(), 2);
    EXPECT_EQ(out.pieEntries[0], (QPair<QString, double>{"Alpha", 30}));
    EXPECT_EQ(out.pieEntries[1], (QPair<QString, double>{"Beta", 70}));
}

TEST(ChartSourceMermaid, Flowchart) {
    const QString diagram =
        "flowchart TD\n"
        "    A[Start]\n"
        "    B(Round)\n"
        "    A-->B\n"
        "    B -- later --> C\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(out.fcDirection, "TD");
    ASSERT_EQ(out.fcNodes.size(), 3);
    EXPECT_EQ(out.fcNodes[0], (QStringList{"A", "Start", "Box"}));
    EXPECT_EQ(out.fcNodes[1], (QStringList{"B", "Round", "Round"}));
    // Bare edge endpoints become auto nodes (mermaid creates them too).
    EXPECT_EQ(out.fcNodes[2], (QStringList{"C", "C", "Box"}));
    ASSERT_EQ(out.fcEdges.size(), 2);
    EXPECT_EQ(out.fcEdges[0], (QStringList{"A", "B", "", "-->"}));
    EXPECT_EQ(out.fcEdges[1], (QStringList{"B", "C", "later", "-->"}));
}

TEST(ChartSourceMermaid, FlowchartFailsWithoutHeader) {
    const QString diagram = "A[Start]\nA --> B\n";
    ChartSource::MermaidData out;
    EXPECT_FALSE(ChartSource::parseMermaid(diagram, out));
}

TEST(ChartSourceMermaid, FlowchartCombinedNodesEdgesAndInlineLabels) {
    const QString diagram =
        "flowchart TD\n"
        "  Start([Start]) --> Auth{Authenticated?}\n"
        "  Auth -->|Yes| Dashboard[Load Dashboard]\n"
        "  Auth -->|No| Login[Login Page]\n"
        "  Login --> Validate{Valid Credentials?}\n"
        "  Validate -->|Yes| Dashboard\n"
        "  Validate -->|No| Error[Show Error]\n"
        "  Error --> Login\n"
        "  Dashboard --> End([End])\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(out.fcDirection, "TD");
    // Seven distinct nodes, in first-appearance order; bare references
    // (Dashboard, Login) must not create duplicates.
    ASSERT_EQ(out.fcNodes.size(), 7);
    EXPECT_EQ(out.fcNodes[0], (QStringList{"Start", "Start", "Stadium"}));
    EXPECT_EQ(out.fcNodes[1], (QStringList{"Auth", "Authenticated?", "Diamond"}));
    EXPECT_EQ(out.fcNodes[2], (QStringList{"Dashboard", "Load Dashboard", "Box"}));
    EXPECT_EQ(out.fcNodes[3], (QStringList{"Login", "Login Page", "Box"}));
    EXPECT_EQ(out.fcNodes[4], (QStringList{"Validate", "Valid Credentials?", "Diamond"}));
    EXPECT_EQ(out.fcNodes[5], (QStringList{"Error", "Show Error", "Box"}));
    EXPECT_EQ(out.fcNodes[6], (QStringList{"End", "End", "Stadium"}));
    ASSERT_EQ(out.fcEdges.size(), 8);
    EXPECT_EQ(out.fcEdges[0], (QStringList{"Start", "Auth", "", "-->"}));
    EXPECT_EQ(out.fcEdges[1], (QStringList{"Auth", "Dashboard", "Yes", "-->"}));
    EXPECT_EQ(out.fcEdges[2], (QStringList{"Auth", "Login", "No", "-->"}));
    EXPECT_EQ(out.fcEdges[3], (QStringList{"Login", "Validate", "", "-->"}));
    EXPECT_EQ(out.fcEdges[4], (QStringList{"Validate", "Dashboard", "Yes", "-->"}));
    EXPECT_EQ(out.fcEdges[5], (QStringList{"Validate", "Error", "No", "-->"}));
    EXPECT_EQ(out.fcEdges[6], (QStringList{"Error", "Login", "", "-->"}));
    EXPECT_EQ(out.fcEdges[7], (QStringList{"Dashboard", "End", "", "-->"}));
}

TEST(ChartSourceMermaid, FlowchartBidirectionalArrows) {
    // Every bidirectional arrow form: solid, thick, dotted, circle, cross —
    // in both compact and spaced-label styles (`A <-- text --> B`).
    const QString diagram =
        "flowchart LR\n"
        "  A <--> B\n"
        "  C <==> D\n"
        "  E <-.-> F\n"
        "  G o--o H\n"
        "  I x--x J\n"
        "  K <-- via --> L\n"
        "  M <== thick ==> N\n"
        "  O <-. dotted .-> P\n"
        "  Q o-- ring --o R\n"
        "  S x-- cross --x T\n"
        "  U <-->|pipe| V\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Flowchart);
    ASSERT_EQ(out.fcEdges.size(), 11);
    EXPECT_EQ(out.fcEdges[0], (QStringList{"A", "B", "", "<-->"}));
    EXPECT_EQ(out.fcEdges[1], (QStringList{"C", "D", "", "<==>"}));
    EXPECT_EQ(out.fcEdges[2], (QStringList{"E", "F", "", "<-.->"}));
    EXPECT_EQ(out.fcEdges[3], (QStringList{"G", "H", "", "o--o"}));
    EXPECT_EQ(out.fcEdges[4], (QStringList{"I", "J", "", "x--x"}));
    EXPECT_EQ(out.fcEdges[5], (QStringList{"K", "L", "via", "<-->"}));
    EXPECT_EQ(out.fcEdges[6], (QStringList{"M", "N", "thick", "<==>"}));
    EXPECT_EQ(out.fcEdges[7], (QStringList{"O", "P", "dotted", "<-.->"}));
    EXPECT_EQ(out.fcEdges[8], (QStringList{"Q", "R", "ring", "o--o"}));
    EXPECT_EQ(out.fcEdges[9], (QStringList{"S", "T", "cross", "x--x"}));
    EXPECT_EQ(out.fcEdges[10], (QStringList{"U", "V", "pipe", "<-->"}));
}

TEST(ChartSourceMermaid, SequenceImplicitParticipantsFromMessages) {
    const QString diagram =
        "sequenceDiagram\n"
        "  Editor->>Parser: send markdown\n"
        "  Parser->>Renderer: produce HTML\n"
        "  Renderer->>Preview: set content\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Sequence);
    ASSERT_EQ(out.seqParticipants.size(), 4);
    EXPECT_EQ(out.seqParticipants[0], (QStringList{"Editor", ""}));
    EXPECT_EQ(out.seqParticipants[1], (QStringList{"Parser", ""}));
    EXPECT_EQ(out.seqParticipants[2], (QStringList{"Renderer", ""}));
    EXPECT_EQ(out.seqParticipants[3], (QStringList{"Preview", ""}));
    ASSERT_EQ(out.seqMessages.size(), 3);
    EXPECT_EQ(out.seqMessages[0], (QStringList{"Editor", "Parser", "send markdown", "->>"}));
    EXPECT_EQ(out.seqMessages[1], (QStringList{"Parser", "Renderer", "produce HTML", "->>"}));
    EXPECT_EQ(out.seqMessages[2], (QStringList{"Renderer", "Preview", "set content", "->>"}));
}

TEST(ChartSourceMermaid, SequenceExplicitThenImplicitDeduplicates) {
    const QString diagram =
        "sequenceDiagram\n"
        "  A->>B: hi\n"
        "  participant Alice as A\n"
        "  participant Bob as B\n"
        "  A->>B: again\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    // A and B are implicit from the first message; the later `participant`
    // lines upgrade them with aliases instead of duplicating.
    ASSERT_EQ(out.seqParticipants.size(), 2);
    EXPECT_EQ(out.seqParticipants[0], (QStringList{"Alice", "A"}));
    EXPECT_EQ(out.seqParticipants[1], (QStringList{"Bob", "B"}));
    ASSERT_EQ(out.seqMessages.size(), 2);
    // Message endpoints that referenced the aliases resolve to the names.
    EXPECT_EQ(out.seqMessages[0], (QStringList{"Alice", "Bob", "hi", "->>"}));
    EXPECT_EQ(out.seqMessages[1], (QStringList{"Alice", "Bob", "again", "->>"}));
}

TEST(ChartSourceMermaid, Sequence) {
    const QString diagram =
        "sequenceDiagram\n"
        "    participant Alice as A\n"
        "    participant Bob as B\n"
        "    Alice->>Bob: Hello\n"
        "    Bob-->>Alice: Hi\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Sequence);
    ASSERT_EQ(out.seqParticipants.size(), 2);
    EXPECT_EQ(out.seqParticipants[0], (QStringList{"Alice", "A"}));
    EXPECT_EQ(out.seqParticipants[1], (QStringList{"Bob", "B"}));
    ASSERT_EQ(out.seqMessages.size(), 2);
    EXPECT_EQ(out.seqMessages[0], (QStringList{"Alice", "Bob", "Hello", "->>"}));
    EXPECT_EQ(out.seqMessages[1], (QStringList{"Bob", "Alice", "Hi", "-->>"}));
}

TEST(ChartSourceMermaid, Gantt) {
    const QString diagram =
        "gantt\n"
        "    title Project\n"
        "    dateFormat YYYY-MM-DD\n"
        "    excludes weekends\n"
        "    section Backend\n"
        "        API design :done, a1, 2026-07-01, 7d\n"
        "        Endpoints :active, a3, after a2, 14d\n"
        "    section Frontend\n"
        "        UI :  b1, 2026-07-10, 10d\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Gantt);
    EXPECT_EQ(out.ganttTitle, "Project");
    EXPECT_EQ(out.ganttDateFormat, "YYYY-MM-DD");
    EXPECT_TRUE(out.ganttWeekend);
    ASSERT_EQ(out.ganttTasks.size(), 3);
    EXPECT_EQ(out.ganttTasks[0],
              (QStringList{"a1", "API design", "2026-07-01", "7d", "done", "Backend"}));
    EXPECT_EQ(out.ganttTasks[1],
              (QStringList{"a3", "Endpoints", "after a2", "14d", "active", "Backend"}));
    EXPECT_EQ(out.ganttTasks[2],
              (QStringList{"b1", "UI", "2026-07-10", "10d", "", "Frontend"}));
}

TEST(ChartSourceMermaid, GanttWithoutExcludesKeepsWeekendFalse) {
    // Reverse-parsing a gantt that never said `excludes weekends` must not
    // silently default it on (otherwise re-inserting adds the line).
    const QString diagram =
        "gantt\n"
        "    dateFormat YYYY-MM-DD\n"
        "    section Dev\n"
        "        A :a1, 2026-07-01, 7d\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_FALSE(out.ganttWeekend);
}

TEST(ChartSourceMermaid, State) {
    const QString diagram =
        "stateDiagram-v2\n"
        "    A --> B\n"
        "    B --> C : done\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::State);
    ASSERT_EQ(out.stateTransitions.size(), 2);
    EXPECT_EQ(out.stateTransitions[0], (QStringList{"A", "B", "", ""}));
    EXPECT_EQ(out.stateTransitions[1], (QStringList{"B", "C", "done", ""}));
}

TEST(ChartSourceMermaid, StateCompositeSections) {
    // Inner transitions of a `state X { ... }` block get that state as their
    // section; transitions outside stay top-level (empty section).
    const QString diagram =
        "stateDiagram-v2\n"
        "    [*] --> Idle\n"
        "    Idle --> Processing : start\n"
        "    state Processing {\n"
        "        [*] --> FetchData\n"
        "        FetchData --> Validate\n"
        "        Validate --> [*]\n"
        "    }\n"
        "    Processing --> Done\n"
        "    Done --> [*]\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::State);
    ASSERT_EQ(out.stateTransitions.size(), 7);
    EXPECT_EQ(out.stateTransitions[0], (QStringList{"[*]", "Idle", "", ""}));
    EXPECT_EQ(out.stateTransitions[1], (QStringList{"Idle", "Processing", "start", ""}));
    EXPECT_EQ(out.stateTransitions[2], (QStringList{"[*]", "FetchData", "", "Processing"}));
    EXPECT_EQ(out.stateTransitions[3], (QStringList{"FetchData", "Validate", "", "Processing"}));
    EXPECT_EQ(out.stateTransitions[4], (QStringList{"Validate", "[*]", "", "Processing"}));
    EXPECT_EQ(out.stateTransitions[5], (QStringList{"Processing", "Done", "", ""}));
}

TEST(ChartSourceMermaid, StateCompositeAliasSection) {
    // `state "description" as X { ... }` — the section uses the alias id X.
    const QString diagram =
        "stateDiagram-v2\n"
        "    [*] --> S\n"
        "    state \"Long name\" as S {\n"
        "        [*] --> Inner\n"
        "        Inner --> [*]\n"
        "    }\n"
        "    S --> [*]\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    ASSERT_EQ(out.stateTransitions.size(), 4);
    EXPECT_EQ(out.stateTransitions[0], (QStringList{"[*]", "S", "", ""}));
    EXPECT_EQ(out.stateTransitions[1], (QStringList{"[*]", "Inner", "", "S"}));
    EXPECT_EQ(out.stateTransitions[2], (QStringList{"Inner", "[*]", "", "S"}));
}

TEST(ChartSourceMermaid, MindmapDialogIndent) {
    // Dialog emits root at depth 1 (4 spaces).
    const QString diagram =
        "mindmap\n"
        "    root((Central Idea))\n"
        "        Idea 1\n"
        "            Sub-idea\n"
        "        Idea 2\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Mindmap);
    ASSERT_EQ(out.mindmapRoots.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].text, "Central Idea");
    ASSERT_EQ(out.mindmapRoots[0].children.size(), 2);
    EXPECT_EQ(out.mindmapRoots[0].children[0].text, "Idea 1");
    ASSERT_EQ(out.mindmapRoots[0].children[0].children.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].children[0].children[0].text, "Sub-idea");
}

TEST(ChartSourceMermaid, MindmapTwoSpaceIndent) {
    // Mermaid's own docs indent with two spaces per level; the old
    // depth = spaces/4 parser flattened every two levels into one.
    const QString diagram =
        "mindmap\n"
        "  root((mindmap))\n"
        "    Origins\n"
        "      Long history\n"
        "      ::icon(fa fa-book)\n"
        "    Research\n"
        "      How to do mindmaps\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    ASSERT_EQ(out.mindmapRoots.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].text, "mindmap");
    ASSERT_EQ(out.mindmapRoots[0].children.size(), 2);
    EXPECT_EQ(out.mindmapRoots[0].children[0].text, "Origins");
    ASSERT_EQ(out.mindmapRoots[0].children[0].children.size(), 2);
    EXPECT_EQ(out.mindmapRoots[0].children[0].children[0].text, "Long history");
    EXPECT_EQ(out.mindmapRoots[0].children[0].children[1].text, "::icon(fa fa-book)");
    EXPECT_EQ(out.mindmapRoots[0].children[1].text, "Research");
    ASSERT_EQ(out.mindmapRoots[0].children[1].children.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].children[1].children[0].text, "How to do mindmaps");
}

TEST(ChartSourceMermaid, MindmapTabIndent) {
    const QString diagram =
        "mindmap\n"
        "\troot((Root))\n"
        "\t\tChild\n"
        "\t\t\tGrandchild\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    ASSERT_EQ(out.mindmapRoots.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].text, "Root");
    ASSERT_EQ(out.mindmapRoots[0].children.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].children[0].text, "Child");
    ASSERT_EQ(out.mindmapRoots[0].children[0].children.size(), 1);
    EXPECT_EQ(out.mindmapRoots[0].children[0].children[0].text, "Grandchild");
}

TEST(ChartSourceMermaid, Timeline) {
    const QString diagram =
        "timeline\n"
        "    title My Timeline\n"
        "    Q1 2026\n"
        "            : Launch v1.0\n"
        "    Q2 2026\n"
        "            : Template library\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Timeline);
    EXPECT_EQ(out.timelineTitle, "My Timeline");
    ASSERT_EQ(out.timelineEntries.size(), 2);
    EXPECT_EQ(out.timelineEntries[0], (QStringList{"Q1 2026", "Launch v1.0"}));
    EXPECT_EQ(out.timelineEntries[1], (QStringList{"Q2 2026", "Template library"}));
}

TEST(ChartSourceMermaid, TimelineCombinedPeriodEventLines) {
    // Mermaid's own docs use `period : event` on one line, with `: event`
    // continuation lines. Each combined line opens its own section.
    const QString diagram =
        "timeline\n"
        "    title Company Milestones\n"
        "    2018 : Founded\n"
        "    2019 : Seed funding\n"
        "         : MVP launch\n"
        "    2020 : Series A\n"
        "         : 10K users\n"
        "    2022 : Series B\n"
        "         : International expansion\n"
        "    2024 : IPO\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Timeline);
    EXPECT_EQ(out.timelineTitle, "Company Milestones");
    const QList<QStringList> expected = {
        {"2018", "Founded"},
        {"2019", "Seed funding"},
        {"2019", "MVP launch"},
        {"2020", "Series A"},
        {"2020", "10K users"},
        {"2022", "Series B"},
        {"2022", "International expansion"},
        {"2024", "IPO"},
    };
    ASSERT_EQ(out.timelineEntries.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i)
        EXPECT_EQ(out.timelineEntries[i], expected[i]);
}

TEST(ChartSourceMermaid, Journey) {
    const QString diagram =
        "journey\n"
        "    title My Day\n"
        "    section Morning\n"
        "        Wake up: 5: Me\n"
        "    section Work\n"
        "        Coding: 7: Me, Team\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Journey);
    EXPECT_EQ(out.journeyTitle, "My Day");
    ASSERT_EQ(out.journeyEntries.size(), 2);
    EXPECT_EQ(out.journeyEntries[0], (QStringList{"Morning", "Wake up", "5", "Me"}));
    EXPECT_EQ(out.journeyEntries[1], (QStringList{"Work", "Coding", "7", "Me, Team"}));
}

TEST(ChartSourceMermaid, Quadrant) {
    const QString diagram =
        "quadrantChart\n"
        "    title Reach and Impact\n"
        "    x-axis Low Reach --> High Reach\n"
        "    y-axis Low Impact --> High Impact\n"
        "    quadrant-1 Quick Wins\n"
        "    Feature A: [0.3, 0.8]\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Quadrant);
    EXPECT_EQ(out.quadTitle, "Reach and Impact");
    EXPECT_EQ(out.quadXLeft, "Low Reach");
    EXPECT_EQ(out.quadXRight, "High Reach");
    EXPECT_EQ(out.quadYBottom, "Low Impact");
    EXPECT_EQ(out.quadYTop, "High Impact");
    EXPECT_EQ(out.quadQ1, "Quick Wins");
    ASSERT_EQ(out.quadPoints.size(), 1);
    EXPECT_EQ(out.quadPoints[0], (QStringList{"Feature A", "0.3", "0.8"}));
}

TEST(ChartSourceMermaid, Sankey) {
    const QString diagram =
        "sankey-beta\n"
        "    Revenue,Product Sales,600\n"
        "    Revenue,Services,300\n";
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Sankey);
    ASSERT_EQ(out.sankeyLinks.size(), 2);
    EXPECT_EQ(out.sankeyLinks[0], (QStringList{"Revenue", "Product Sales", "600"}));
}

TEST(ChartSourceMermaid, ClassFallsBackToRaw) {
    const QString diagram = "classDiagram\n    class Animal\n";
    ChartSource::MermaidData out;
    EXPECT_FALSE(ChartSource::parseMermaid(diagram, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Class);
    EXPECT_EQ(out.source, diagram);
}

} // namespace
