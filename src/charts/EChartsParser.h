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
#pragma once

// Reverse-parse the source Scriba's ECharts helpers emit back into the data the
// helper dialogs consume, so an existing rendered chart can be re-opened in its
// dialog with the fields pre-filled. Pure logic — no Qt widgets — so it is
// unit-testable without WebEngine.
//
// Handles Bar/Line/Area/Scatter/Effect Scatter/Pictorial Bar/Pie/Funnel/Gauge
// (Chart Dialog), Radar and both Heatmap flavours (matrix + calendar), plus
// candlestick (Stock Chart Dialog) and the Advanced Charts Dialog set
// (sankey, boxplot, parallel, themeRiver, graph, treemap, sunburst).
// Other series types fall back to raw-source editing.

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace ChartSource {

// ---------------------------------------------------------------------------
// Stock chart engines (` ```lc ` / ` ```kc ` / ` ```tx ` fences, plus the
// original ` ```ec ` ECharts path). The dialog emits a one-line call
// `scribaStockChart("engine", {payload})`; each engine variant strips that
// wrapper and fills the SAME StockSpecData.
// ---------------------------------------------------------------------------

enum class StockEngine { Unknown, ECharts, Lightweight, KlineCharts, TradeX };
enum class StockChartType { Candlestick, Bar, Line, Area }; // payload "type"; default Candlestick

// Detect the engine from a fence body: pure ECharts JSON (has "series") ->
// ECharts; a `scribaStockChart("engine", ...)` call -> the named engine.
StockEngine detectStockEngine(const QByteArray &fenceBody);

// ---------------------------------------------------------------------------
// ECharts (` ```ec ` blocks)
// ---------------------------------------------------------------------------

enum class EcType { Unknown, Chart, Stock, Sankey, Boxplot, Parallel, ThemeRiver, Graph, Treemap, Sunburst };

// Distinguishes a generic Chart Builder spec from a Stock Chart spec
// (series[0].type == "candlestick") or one of the Advanced Charts Dialog types
// (series[0].type == "sankey" | "boxplot" | "parallel" | "themeRiver" | "graph"
// | "treemap" | "sunburst").
EcType detectEcType(const QByteArray &specJson);

// Data for a generic Chart Builder spec, reconstructed so the dialog can
// repopulate its table + options and rebuild an equivalent spec.
struct ChartSpecData {
    QString type;           // bar | line | area | scatter | effectScatter
                            // | pictorialBar | pie | funnel | gauge
                            // | radar | heatmap (matrix) | calendar (heatmap)
    QString title;
    bool tooltip = false;
    bool animate = true;
    bool rippleEffect = true;  // effectScatter only: emit rippleEffect/symbolSize
    bool repeatSymbol = true;  // pictorialBar only: emit symbolRepeat styling
    QStringList headers;    // 2-3 column headers (Label/Value, Category/Value, X/Y, Date/Value, Indicator/Value/Max, X/Y/Value)
    QList<QStringList> rows; // data rows, one cell per header
};

// Returns true when `specJson` is a Chart Builder-style spec (not candlestick)
// that could be reconstructed.
bool parseChartSpec(const QByteArray &specJson, ChartSpecData &out);

// Data for a Stock Chart spec (candlestick).
struct StockSpecData {
    QString title;
    bool volume = false;
    bool zoom = false;
    bool animate = true;
    bool ma5 = false, ma10 = false, ma20 = false, ma50 = false;
    QStringList dates;
    QList<QList<double>> ohlc; // per bar: [open, close, low, high]
    QList<double> volumes;     // parallel to dates; empty entries are null
    bool hasVolume = false;
    StockChartType chartType = StockChartType::Candlestick;
    bool macd = false, rsi = false, boll = false, kdj = false;
};

bool parseStockSpec(const QByteArray &specJson, StockSpecData &out);

// The non-ECharts engines strip the `scribaStockChart("engine", ...)` wrapper
// and parse the shared payload into the SAME StockSpecData as parseStockSpec.
bool parseLightweightSpec(const QByteArray &payloadJson, StockSpecData &out);
bool parseKlinechartsSpec(const QByteArray &payloadJson, StockSpecData &out);
bool parseTradexSpec(const QByteArray &payloadJson, StockSpecData &out);

// ---------------------------------------------------------------------------
// Advanced Charts Dialog types (sankey / boxplot / parallel / themeRiver /
// graph / treemap / sunburst). Each reverses the ECharts JSON the dialog
// emits back into the dialog's own table rows.
// ---------------------------------------------------------------------------

struct SankeySpecData {
    QString title;
    bool animate = true;
    QList<QStringList> links; // {source, target, weight}
};

bool parseSankeySpec(const QByteArray &specJson, SankeySpecData &out);

struct BoxplotSpecData {
    QString title;
    bool animate = true;
    QStringList categories;      // x-axis category names
    QList<QList<double>> stats;  // per category: [min, q1, median, q3, max]
};

bool parseBoxplotSpec(const QByteArray &specJson, BoxplotSpecData &out);

struct ParallelSpecData {
    QString title;
    bool animate = true;
    QStringList dimensions;      // parallel-axis dimension names, in axis order
    QList<QList<double>> lines;   // per parallel line: one value per dimension
};

bool parseParallelSpec(const QByteArray &specJson, ParallelSpecData &out);

struct ThemeRiverSpecData {
    QString title;
    bool animate = true;
    QList<QStringList> rows; // {date, value, category}
};

bool parseThemeRiverSpec(const QByteArray &specJson, ThemeRiverSpecData &out);

struct GraphSpecData {
    QString title;
    bool animate = true;
    QStringList nodeNames;   // graph nodes, in order
    QList<double> nodeValues; // parallel to nodeNames (0 when absent)
    QList<QStringList> links; // {source, target, value}
};

bool parseGraphSpec(const QByteArray &specJson, GraphSpecData &out);

// Reverse model for treemap + sunburst. `rows` holds one entry per leaf node:
// the node-name path from the root (one cell per depth level) followed by the
// value in the final cell. Internal nodes are inferred from the paths, so an
// internal node's own value (when it also has children) is not recoverable.
struct TreeSpecData {
    QString title;
    bool animate = true;
    QList<QStringList> rows; // per leaf: [name, ..., name, value]
};

bool parseTreeSpec(const QByteArray &specJson, TreeSpecData &out);

} // namespace ChartSource
