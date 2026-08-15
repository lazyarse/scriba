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
// array-aligned with the date axis by construction. The math is delegated to
// the vendored TA-Lib C library (see vendor/ta-lib); the outBegIdx/outNBElement
// offsets TA-Lib reports are re-aligned into NaN-prefixed arrays here.

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

// diff = ema(fast) - ema(slow); dea = ema(signal) of diff, seeded with the SMA
// of the first `signal` macd values; hist = diff - dea (TA-Lib conventions).
// NaN for i < (slow-1)+(signal-1).
MacdSeries macd(const QList<double> &close, int fast = 12, int slow = 26, int signal = 9);

// Wilder's smoothing, per TA-Lib: RSI = 100*gain/(gain+loss), 0.0 when the
// gain+loss window is flat; NaN for i < period.
QVector<double> rsi(const QList<double> &close, int period = 14);

// TA-Lib BBANDS with SMA middle band and population stddev: mid = sma(period);
// upper/lower = mid ± mult * stddev.
BollSeries boll(const QList<double> &close, int period = 20, double mult = 2.0);

// TA-Lib has no KDJ; reimplemented via TA_STOCH (fastK = n, slowK = k, slowD
// = d, both smoothed with SMA) with j = 3k - 2d computed locally. Values and
// alignment differ from a traditional KDJ: NaN for i < (n-1)+(k-1)+(d-1).
KdjSeries kdj(const QList<double> &close, const QList<double> &high,
              const QList<double> &low, int n = 9, int k = 3, int d = 3);

} // namespace Indicators
