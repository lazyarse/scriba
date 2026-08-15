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

#include "ta_func.h"

#include <cmath>
#include <limits>
#include <vector>

namespace Indicators {

namespace {

// TA-Lib reports its results via outBegIdx/outNBElement (the first valid index
// and the number of valid elements starting there), not as NaN-prefixed
// arrays. Re-align into the NaN-prefixed QVector of inputSize that the rest of
// the API and the chart payload expect.
QVector<double> align(const double *out, int outBegIdx, int outNbElement, int inputSize)
{
    QVector<double> result(inputSize, std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < outNbElement; ++i) {
        const int idx = outBegIdx + i;
        if (idx >= 0 && idx < inputSize)
            result[idx] = out[i];
    }
    return result;
}

QVector<double> toInput(const QList<double> &values)
{
    QVector<double> input(values.size());
    for (int i = 0; i < values.size(); ++i)
        input[i] = values[i];
    return input;
}

} // namespace

QVector<double> sma(const QList<double> &values, int period)
{
    const QVector<double> in = toInput(values);
    QVector<double> out(in.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_SMA(0, in.size() - 1, in.constData(), period, &outBegIdx, &outNbElement,
           out.data());
    return align(out.constData(), outBegIdx, outNbElement, in.size());
}

QVector<double> ema(const QList<double> &values, int period)
{
    const QVector<double> in = toInput(values);
    QVector<double> out(in.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_EMA(0, in.size() - 1, in.constData(), period, &outBegIdx, &outNbElement,
           out.data());
    return align(out.constData(), outBegIdx, outNbElement, in.size());
}

MacdSeries macd(const QList<double> &close, int fast, int slow, int signal)
{
    const QVector<double> in = toInput(close);
    QVector<double> diff(in.size()), dea(in.size()), hist(in.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_MACD(0, in.size() - 1, in.constData(), fast, slow, signal, &outBegIdx,
            &outNbElement, diff.data(), dea.data(), hist.data());
    // TA-Lib's histogram output is macd - signal already (outMACDHist[i] =
    // outMACD[i] - outMACDSignal[i]); align it as-is.
    return {align(diff.constData(), outBegIdx, outNbElement, in.size()),
            align(dea.constData(), outBegIdx, outNbElement, in.size()),
            align(hist.constData(), outBegIdx, outNbElement, in.size())};
}

QVector<double> rsi(const QList<double> &close, int period)
{
    const QVector<double> in = toInput(close);
    QVector<double> out(in.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_RSI(0, in.size() - 1, in.constData(), period, &outBegIdx, &outNbElement,
           out.data());
    return align(out.constData(), outBegIdx, outNbElement, in.size());
}

BollSeries boll(const QList<double> &close, int period, double mult)
{
    const QVector<double> in = toInput(close);
    QVector<double> upper(in.size()), mid(in.size()), lower(in.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_BBANDS(0, in.size() - 1, in.constData(), period, mult, mult,
              TA_MAType_SMA, &outBegIdx, &outNbElement, upper.data(),
              mid.data(), lower.data());
    return {align(upper.constData(), outBegIdx, outNbElement, in.size()),
            align(mid.constData(), outBegIdx, outNbElement, in.size()),
            align(lower.constData(), outBegIdx, outNbElement, in.size())};
}

KdjSeries kdj(const QList<double> &close, const QList<double> &high,
              const QList<double> &low, int n, int k, int d)
{
    const QVector<double> inClose = toInput(close);
    const QVector<double> inHigh = toInput(high);
    const QVector<double> inLow = toInput(low);
    QVector<double> kOut(inClose.size()), dOut(inClose.size());
    int outBegIdx = 0;
    int outNbElement = 0;
    TA_STOCH(0, inClose.size() - 1, inHigh.constData(), inLow.constData(),
             inClose.constData(), n, k, TA_MAType_SMA, d, TA_MAType_SMA,
             &outBegIdx, &outNbElement, kOut.data(), dOut.data());
    const QVector<double> ks = align(kOut.constData(), outBegIdx, outNbElement,
                                     inClose.size());
    const QVector<double> ds = align(dOut.constData(), outBegIdx, outNbElement,
                                     inClose.size());
    // TA-Lib has no J line; KDJ's J = 3K - 2D is computed locally.
    QVector<double> js(ks.size(), std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < ks.size(); ++i) {
        if (!std::isnan(ks[i]) && !std::isnan(ds[i]))
            js[i] = 3.0 * ks[i] - 2.0 * ds[i];
    }
    return {ks, ds, js};
}

} // namespace Indicators