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
#include "Indicators.h"

#include <cmath>
#include <limits>

namespace Indicators {

QVector<double> sma(const QList<double> &values, int period)
{
    QVector<double> result(values.size());
    double sum = 0;
    for (int i = 0; i < values.size(); ++i) {
        sum += values[i];
        if (i >= period)
            sum -= values[i - period];
        result[i] = i >= period - 1 ? sum / period
                                     : std::numeric_limits<double>::quiet_NaN();
    }
    return result;
}

QVector<double> ema(const QList<double> &values, int period)
{
    QVector<double> result(values.size());
    const double k = 2.0 / (period + 1);
    double prev = 0;
    bool seeded = false;
    for (int i = 0; i < values.size(); ++i) {
        if (!seeded) {
            if (i < period - 1) {
                result[i] = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            double sum = 0;
            for (int j = i - period + 1; j <= i; ++j)
                sum += values[j];
            prev = sum / period;
            seeded = true;
        } else {
            prev = (values[i] - prev) * k + prev;
        }
        result[i] = prev;
    }
    return result;
}

MacdSeries macd(const QList<double> &close, int fast, int slow, int signal)
{
    const QVector<double> emaFast = ema(close, fast);
    const QVector<double> emaSlow = ema(close, slow);

    QVector<double> diff(close.size(), std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < close.size(); ++i) {
        if (i >= slow - 1 && i >= fast - 1)
            diff[i] = emaFast[i] - emaSlow[i];
    }

    // dea = ema(signal) of the diff series (NaN-aware seed: first valid diff).
    QVector<double> dea(close.size(), std::numeric_limits<double>::quiet_NaN());
    const double k = 2.0 / (signal + 1);
    double prev = 0;
    int seeded = -1;
    for (int i = 0; i < close.size(); ++i) {
        if (std::isnan(diff[i]))
            continue;
        if (seeded < 0) {
            prev = diff[i];
            seeded = i;
        } else {
            prev = (diff[i] - prev) * k + prev;
        }
        dea[i] = prev;
    }

    QVector<double> hist(close.size(), std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < close.size(); ++i) {
        if (!std::isnan(dea[i]))
            hist[i] = 2.0 * (diff[i] - dea[i]);
    }
    return {diff, dea, hist};
}

QVector<double> rsi(const QList<double> &close, int period)
{
    QVector<double> result(close.size(), std::numeric_limits<double>::quiet_NaN());
    if (close.size() <= period)
        return result;

    double avgGain = 0, avgLoss = 0;
    for (int i = 1; i <= period; ++i) {
        const double delta = close[i] - close[i - 1];
        if (delta > 0)
            avgGain += delta;
        else
            avgLoss -= delta;
    }
    avgGain /= period;
    avgLoss /= period;
    result[period] = avgLoss == 0.0
        ? 100.0
        : 100.0 - 100.0 / (1.0 + avgGain / avgLoss);

    for (int i = period + 1; i < close.size(); ++i) {
        const double delta = close[i] - close[i - 1];
        avgGain = (avgGain * (period - 1) + (delta > 0 ? delta : 0.0)) / period;
        avgLoss = (avgLoss * (period - 1) + (delta < 0 ? -delta : 0.0)) / period;
        result[i] = avgLoss == 0.0
            ? 100.0
            : 100.0 - 100.0 / (1.0 + avgGain / avgLoss);
    }
    return result;
}

BollSeries boll(const QList<double> &close, int period, double mult)
{
    const QVector<double> mid = sma(close, period);
    QVector<double> upper(close.size()), lower(close.size());
    for (int i = 0; i < close.size(); ++i) {
        if (std::isnan(mid[i])) {
            upper[i] = lower[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        double variance = 0;
        for (int j = i - period + 1; j <= i; ++j) {
            const double d = close[j] - mid[i];
            variance += d * d;
        }
        const double stddev = std::sqrt(variance / period); // population
        upper[i] = mid[i] + mult * stddev;
        lower[i] = mid[i] - mult * stddev;
    }
    return {upper, mid, lower};
}

KdjSeries kdj(const QList<double> &close, const QList<double> &high,
              const QList<double> &low, int n, int k, int d)
{
    const int size = close.size();
    QVector<double> ks(size, std::numeric_limits<double>::quiet_NaN());
    QVector<double> ds(size, std::numeric_limits<double>::quiet_NaN());
    QVector<double> js(size, std::numeric_limits<double>::quiet_NaN());

    double kPrev = 50.0, dPrev = 50.0;
    const double kWeight = (k - 1.0) / k;
    const double dWeight = (d - 1.0) / d;
    for (int i = n - 1; i < size; ++i) {
        double hhv = -std::numeric_limits<double>::infinity();
        double llv = std::numeric_limits<double>::infinity();
        for (int j = i - n + 1; j <= i; ++j) {
            hhv = std::max(hhv, high[j]);
            llv = std::min(llv, low[j]);
        }
        const double range = hhv - llv;
        double kCur = kPrev;
        if (range > 0) {
            const double rsv = (close[i] - llv) / range * 100.0;
            kCur = kPrev * kWeight + rsv / k;
        }
        const double dCur = dPrev * dWeight + kCur / d;
        kPrev = kCur;
        dPrev = dCur;
        ks[i] = kCur;
        ds[i] = dCur;
        js[i] = 3.0 * kCur - 2.0 * dCur;
    }
    return {ks, ds, js};
}

} // namespace Indicators
