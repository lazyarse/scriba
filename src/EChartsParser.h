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

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace ChartSource {

// ---------------------------------------------------------------------------
// ECharts (` ```ec ` blocks)
// ---------------------------------------------------------------------------

enum class EcType { Unknown, Chart, Stock };

// Distinguishes a Stock Chart spec (series[0].type == "candlestick") from a
// generic Chart Builder spec.
EcType detectEcType(const QByteArray &specJson);

// Data for a generic Chart Builder spec, reconstructed so the dialog can
// repopulate its table + options and rebuild an equivalent spec.
struct ChartSpecData {
    QString type;           // bar | line | area | scatter | pie
    QString title;
    bool tooltip = false;
    bool animate = true;
    QStringList headers;    // 2 column headers (Label/Value, Category/Value, X/Y…)
    QList<QStringList> rows; // data rows, 2 cells each
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
};

bool parseStockSpec(const QByteArray &specJson, StockSpecData &out);

} // namespace ChartSource
