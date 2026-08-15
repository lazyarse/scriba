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

// Single C++ source of truth for stock-chart indicator math. Every function
// returns a NaN-prefixed series with length == input length, so all consumers
// (the shared chart payload, the JS adapters, the spec parsers) stay
// array-aligned with the date axis by construction. Hand-rolled for now; a
// future TA-Lib swap touches ONLY this file.

#include <QList>
#include <QVector>

namespace Indicators {

struct MacdSeries {
    QVector<double> diff, dea, hist;
};
struct BollSeries {
    QVector<double> upper, mid, lower;
};
struct KdjSeries {
    QVector<double> k, d, j;
};

// NaN for i < period-1, sliding-window mean afterwards. MUST stay
// value-identical to the removed StockChartDialog::movingAverage.
QVector<double> sma(const QList<double> &values, int period);

// Seeded at sma(period) of the first `period` values, then
// (v - prev) * k + prev with k = 2/(period+1).
QVector<double> ema(const QList<double> &values, int period);

// diff = ema(fast) - ema(slow); dea = ema(signal) of diff;
// hist = 2 * (diff - dea) (common display convention).
MacdSeries macd(const QList<double> &close, int fast = 12, int slow = 26, int signal = 9);

// Wilder's smoothing: first value from the average gain/loss over the first
// `period` deltas, then RMA smoothing; NaN for i < period.
QVector<double> rsi(const QList<double> &close, int period = 14);

// mid = sma(period); upper/lower = mid ± mult * population stddev.
BollSeries boll(const QList<double> &close, int period = 20, double mult = 2.0);

// RSV over an n-period window; K/D smoothed as prev*(s-1)/s + x/s with the
// given factors; j = 3k - 2d. NaN for i < n-1.
KdjSeries kdj(const QList<double> &close, const QList<double> &high,
              const QList<double> &low, int n = 9, int k = 3, int d = 3);

} // namespace Indicators
