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
#include "EChartsParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace ChartSource {

// ---------------------------------------------------------------------------
// ECharts
// ---------------------------------------------------------------------------

static QString seriesType(const QJsonObject &spec, int index)
{
    QJsonValue s = spec.value("series").toArray().at(index);
    return s.toObject().value("type").toString();
}

EcType detectEcType(const QByteArray &specJson)
{
    QJsonDocument doc = QJsonDocument::fromJson(specJson);
    if (!doc.isObject())
        return EcType::Unknown;
    const QJsonObject spec = doc.object();
    const QJsonArray series = spec.value("series").toArray();
    if (series.isEmpty())
        return EcType::Unknown;
    if (seriesType(spec, 0) == QLatin1String("candlestick"))
        return EcType::Stock;
    const QString type = seriesType(spec, 0);
    if (type == QLatin1String("bar") || type == QLatin1String("line")
        || type == QLatin1String("scatter") || type == QLatin1String("pie")
        || type == QLatin1String("funnel") || type == QLatin1String("gauge")
        || type == QLatin1String("radar") || type == QLatin1String("heatmap"))
        return EcType::Chart;
    return EcType::Unknown;
}

static QJsonObject axisObject(const QJsonObject &spec, const char *key)
{
    const QJsonValue v = spec.value(QLatin1String(key));
    if (v.isObject())
        return v.toObject();
    return v.toArray().at(0).toObject();
}

bool parseChartSpec(const QByteArray &specJson, ChartSpecData &out)
{
    QJsonDocument doc = QJsonDocument::fromJson(specJson);
    if (!doc.isObject())
        return false;
    const QJsonObject spec = doc.object();
    const QJsonArray series = spec.value("series").toArray();
    if (series.isEmpty())
        return false;

    if (!spec.value("animation").toBool(true))
        out.animate = false;

    const QJsonObject title = spec.value("title").toObject();
    if (title.contains("text"))
        out.title = title.value("text").toString();

    if (spec.value("tooltip").isObject())
        out.tooltip = true;

    const QJsonObject s0 = series.at(0).toObject();
    const QString type = s0.value("type").toString();
    if (type == QLatin1String("candlestick"))
        return false;
    if (type == QLatin1String("bar")) out.type = "bar";
    else if (type == QLatin1String("scatter")) out.type = "scatter";
    else if (type == QLatin1String("pie")) out.type = "pie";
    else if (type == QLatin1String("line"))
        out.type = s0.contains("areaStyle") ? "area" : "line";
    else if (type == QLatin1String("funnel") || type == QLatin1String("gauge"))
        out.type = type;
    else if (type == QLatin1String("radar"))
        out.type = "radar";
    else if (type == QLatin1String("heatmap"))
        out.type = s0.value("coordinateSystem").toString() == QLatin1String("calendar")
            ? "calendar" : "heatmap";
    else
        return false;

    const QJsonValue s0data = s0.value("data");

    // Name/value item charts: pie, funnel, gauge.
    if (out.type == QLatin1String("pie")
        || out.type == QLatin1String("funnel")
        || out.type == QLatin1String("gauge")) {
        out.headers = {QStringLiteral("Label"), QStringLiteral("Value")};
        for (const QJsonValue &v : s0data.toArray()) {
            const QJsonObject item = v.toObject();
            const QString name = item.value("name").toString();
            if (name.isEmpty())
                continue;
            out.rows.append({name, QString::number(item.value("value").toDouble())});
        }
        return !out.rows.isEmpty();
    }

    // Radar: one series of indicator values, maxes from radar.indicator.
    if (out.type == QLatin1String("radar")) {
        const QJsonArray indicators =
            spec.value("radar").toObject().value("indicator").toArray();
        const QJsonArray data = s0data.toArray();
        const QJsonValue first = data.isEmpty() ? QJsonValue() : data.at(0);
        const QJsonArray values = first.toObject().value("value").toArray();
        if (indicators.isEmpty() || indicators.size() != values.size())
            return false;
        out.headers = {QStringLiteral("Indicator"), QStringLiteral("Value"), QStringLiteral("Max")};
        for (int i = 0; i < indicators.size(); ++i) {
            const QJsonObject ind = indicators.at(i).toObject();
            const QString name = ind.value("name").toString();
            if (name.isEmpty())
                continue;
            out.rows.append({name,
                             QString::number(values.at(i).toDouble()),
                             QString::number(ind.value("max").toDouble())});
        }
        return !out.rows.isEmpty();
    }

    // Calendar heatmap: [date, value] pairs on a calendar coordinate system.
    if (out.type == QLatin1String("calendar")) {
        out.headers = {QStringLiteral("Date"), QStringLiteral("Value")};
        for (const QJsonValue &v : s0data.toArray()) {
            const QJsonArray pair = v.toArray();
            if (pair.size() < 2)
                continue;
            const QString date = pair.at(0).toString();
            if (date.isEmpty())
                continue;
            out.rows.append({date, QString::number(pair.at(1).toDouble())});
        }
        return !out.rows.isEmpty();
    }

    // Matrix heatmap: [xIdx, yIdx, value] triples over two category axes.
    if (out.type == QLatin1String("heatmap")) {
        const QJsonArray xCats = axisObject(spec, "xAxis").value("data").toArray();
        const QJsonArray yCats = axisObject(spec, "yAxis").value("data").toArray();
        out.headers = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Value")};
        for (const QJsonValue &v : s0data.toArray()) {
            const QJsonArray triple = v.toArray();
            if (triple.size() < 3)
                continue;
            const int xi = triple.at(0).toInt();
            const int yi = triple.at(1).toInt();
            const QJsonValue xQ = xi >= 0 && xi < xCats.size() ? xCats.at(xi) : QJsonValue();
            const QJsonValue yQ = yi >= 0 && yi < yCats.size() ? yCats.at(yi) : QJsonValue();
            const QString xCat = xQ.toString();
            const QString yCat = yQ.toString();
            if (xCat.isEmpty() || yCat.isEmpty())
                continue;
            out.rows.append({xCat, yCat, QString::number(triple.at(2).toDouble())});
        }
        return !out.rows.isEmpty();
    }

    const QJsonObject xAxis = axisObject(spec, "xAxis");
    const QJsonArray xData = xAxis.value("data").toArray();

    if (!xData.isEmpty()) {
        // Category x-axis: xAxis.data + flat series.data.
        out.headers = {QStringLiteral("Category"), QStringLiteral("Value")};
        for (int i = 0; i < xData.size(); ++i) {
            const QString cat = xData.at(i).toString();
            const QJsonValue val = s0data.toArray().at(i);
            if (val.isUndefined() || val.isNull())
                continue;
            out.rows.append({cat, QString::number(val.toDouble())});
        }
    } else if (s0data.isArray()) {
        const QJsonArray data = s0data.toArray();
        if (!data.isEmpty() && data.at(0).isArray()) {
            // [x, y] pairs (numeric x-axis; the dialog omits xAxis then).
            out.headers = {QStringLiteral("X"), QStringLiteral("Y")};
            for (const QJsonValue &v : data) {
                const QJsonArray pair = v.toArray();
                if (pair.size() < 2)
                    continue;
                out.rows.append({QString::number(pair.at(0).toDouble()),
                                 QString::number(pair.at(1).toDouble())});
            }
        } else {
            // No x data and flat values: use the row index as the category.
            out.headers = {QStringLiteral("Category"), QStringLiteral("Value")};
            for (int i = 0; i < data.size(); ++i) {
                const QJsonValue val = data.at(i);
                if (val.isNull())
                    continue;
                out.rows.append({QString::number(i), QString::number(val.toDouble())});
            }
        }
    } else {
        return false;
    }

    return !out.rows.isEmpty();
}

bool parseStockSpec(const QByteArray &specJson, StockSpecData &out)
{
    QJsonDocument doc = QJsonDocument::fromJson(specJson);
    if (!doc.isObject())
        return false;
    const QJsonObject spec = doc.object();
    const QJsonArray series = spec.value("series").toArray();

    if (!spec.value("animation").toBool(true))
        out.animate = false;

    const QJsonObject title = spec.value("title").toObject();
    if (title.contains("text"))
        out.title = title.value("text").toString();

    out.zoom = spec.contains("dataZoom");

    // Accept the axis as either an object or a single-element array (the dialog
    // emits the array form; hand-written charts often use the object form).
    const QJsonValue xAxisVal = spec.value("xAxis");
    const QJsonObject xAxis = xAxisVal.isArray()
        ? xAxisVal.toArray().at(0).toObject() : xAxisVal.toObject();
    const QJsonArray xData = xAxis.value("data").toArray();
    for (const QJsonValue &v : xData)
        out.dates.append(v.toString());
    if (out.dates.isEmpty())
        return false;

    const QJsonArray grid = spec.value("grid").toArray();
    out.volume = grid.size() > 1;

    for (const QJsonValue &sv : series) {
        const QJsonObject s = sv.toObject();
        const QString type = s.value("type").toString();
        const QString name = s.value("name").toString();
        if (type == QLatin1String("candlestick")) {
            for (const QJsonValue &v : s.value("data").toArray()) {
                const QJsonArray ohlc = v.toArray();
                QList<double> row;
                for (int i = 0; i < 4 && i < ohlc.size(); ++i)
                    row.append(ohlc.at(i).toDouble());
                if (row.size() == 4)
                    out.ohlc.append(row);
            }
        } else if (type == QLatin1String("bar") && name == QLatin1String("Volume")) {
            out.hasVolume = true;
            for (const QJsonValue &v : s.value("data").toArray())
                out.volumes.append(v.isNull() ? 0.0 : v.toDouble());
        } else if (type == QLatin1String("line")) {
            if (name == QLatin1String("MA5")) out.ma5 = true;
            else if (name == QLatin1String("MA10")) out.ma10 = true;
            else if (name == QLatin1String("MA20")) out.ma20 = true;
            else if (name == QLatin1String("MA50")) out.ma50 = true;
        }
    }

    if (out.ohlc.size() != out.dates.size())
        return false;
    if (out.hasVolume && out.volumes.size() != out.dates.size())
        return false;
    return !out.ohlc.isEmpty();
}

} // namespace ChartSource
