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
#include <cmath>
#include <QVector>

#include "charts/Indicators.h"

using Indicators::BollSeries;
using Indicators::KdjSeries;
using Indicators::MacdSeries;

namespace {

bool isNan(double v) { return std::isnan(v); }

} // namespace

TEST(Indicators, SmaParityWithOldMovingAverage)
{
    // Reference semantics of the removed StockChartDialog::movingAverage:
    // sliding window sum, NaN for i < period-1, sum/period otherwise.
    const QList<double> input = {1, 2, 3, 4, 5, 6};
    const QVector<double> out = Indicators::sma(input, 3);
    ASSERT_EQ(input.size(), out.size());
    EXPECT_TRUE(isNan(out[0]));
    EXPECT_TRUE(isNan(out[1]));
    EXPECT_DOUBLE_EQ(2.0, out[2]);
    EXPECT_DOUBLE_EQ(3.0, out[3]);
    EXPECT_DOUBLE_EQ(4.0, out[4]);
    EXPECT_DOUBLE_EQ(5.0, out[5]);
}

TEST(Indicators, SmaParityWithOldMovingAverageLongerWindow)
{
    const QList<double> input = {5, 7, 2, 9, 3, 8, 4};
    const QVector<double> out = Indicators::sma(input, 4);
    ASSERT_EQ(input.size(), out.size());
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(isNan(out[i]));
    EXPECT_DOUBLE_EQ((5 + 7 + 2 + 9) / 4.0, out[3]);
    EXPECT_DOUBLE_EQ((7 + 2 + 9 + 3) / 4.0, out[4]);
    EXPECT_DOUBLE_EQ((2 + 9 + 3 + 8) / 4.0, out[5]);
    EXPECT_DOUBLE_EQ((9 + 3 + 8 + 4) / 4.0, out[6]);
}

TEST(Indicators, EmaKnownValues)
{
    // k = 2/(period+1) = 0.5; seed = sma of the first 3 values = 7/3.
    const QList<double> input = {1, 4, 2, 8, 5, 9};
    const QVector<double> out = Indicators::ema(input, 3);
    ASSERT_EQ(input.size(), out.size());
    EXPECT_TRUE(isNan(out[0]));
    EXPECT_TRUE(isNan(out[1]));
    EXPECT_DOUBLE_EQ(7.0 / 3.0, out[2]);
    EXPECT_NEAR(31.0 / 6.0, out[3], 1e-12);
    EXPECT_NEAR(61.0 / 12.0, out[4], 1e-12);
    EXPECT_NEAR(169.0 / 24.0, out[5], 1e-12);
}

TEST(Indicators, MacdShapesAndKnownValues)
{
    // Degenerate-but-exact series: fast=2, slow=4, signal=2. All emas are
    // exact halves, so diff/dea land on exact values and hist on exact zeros.
    const QList<double> input = {1, 2, 3, 4, 5, 6};
    const MacdSeries m = Indicators::macd(input, 2, 4, 2);
    ASSERT_EQ(input.size(), m.diff.size());
    ASSERT_EQ(input.size(), m.dea.size());
    ASSERT_EQ(input.size(), m.hist.size());
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(isNan(m.diff[i]));
        EXPECT_TRUE(isNan(m.dea[i]));
        EXPECT_TRUE(isNan(m.hist[i]));
    }
    for (int i = 3; i < input.size(); ++i) {
        EXPECT_DOUBLE_EQ(1.0, m.diff[i]);
        EXPECT_DOUBLE_EQ(1.0, m.dea[i]);
        EXPECT_DOUBLE_EQ(0.0, m.hist[i]);
    }
}

TEST(Indicators, MacdConstantSeriesAllZero)
{
    const QList<double> input = {5, 5, 5, 5, 5, 5, 5, 5};
    const MacdSeries m = Indicators::macd(input, 2, 4, 2);
    ASSERT_EQ(input.size(), m.diff.size());
    for (int i = 4; i < input.size(); ++i) {
        EXPECT_DOUBLE_EQ(0.0, m.diff[i]);
        EXPECT_DOUBLE_EQ(0.0, m.dea[i]);
        EXPECT_DOUBLE_EQ(0.0, m.hist[i]);
    }
}

TEST(Indicators, RsiKnownValues)
{
    // Monotonic increase: no losses, RSI must be exactly 100 after warmup.
    QList<double> up;
    for (int i = 1; i <= 20; ++i)
        up.append(i);
    const QVector<double> rsiUp = Indicators::rsi(up, 14);
    ASSERT_EQ(up.size(), rsiUp.size());
    for (int i = 0; i < 14; ++i)
        EXPECT_TRUE(isNan(rsiUp[i]));
    for (int i = 14; i < 20; ++i)
        EXPECT_DOUBLE_EQ(100.0, rsiUp[i]);

    // 13 up-days of 1 then one down-day of 1: avgGain=13/14, avgLoss=1/14.
    QList<double> mixed;
    for (int i = 0; i < 14; ++i)
        mixed.append(i + 1);
    mixed.append(13); // values 1..14 then 13: 13 gains, 1 loss
    const QVector<double> rsiMix = Indicators::rsi(mixed, 14);
    ASSERT_EQ(15, rsiMix.size());
    for (int i = 0; i < 14; ++i)
        EXPECT_TRUE(isNan(rsiMix[i]));
    EXPECT_NEAR(100.0 - 100.0 / 14.0, rsiMix[14], 1e-9);
}

TEST(Indicators, BollBands)
{
    const QList<double> input = {1, 2, 3, 4, 5, 6};
    const BollSeries b = Indicators::boll(input, 3, 2.0);
    ASSERT_EQ(input.size(), b.upper.size());
    ASSERT_EQ(input.size(), b.mid.size());
    ASSERT_EQ(input.size(), b.lower.size());
    EXPECT_TRUE(isNan(b.upper[0]));
    EXPECT_TRUE(isNan(b.mid[0]));
    EXPECT_TRUE(isNan(b.lower[0]));
    EXPECT_TRUE(isNan(b.upper[1]));
    // mid == sma(period)
    EXPECT_DOUBLE_EQ(2.0, b.mid[2]);
    EXPECT_DOUBLE_EQ(3.0, b.mid[3]);
    // population stddev at index 2: sqrt(((1-2)^2+(2-2)^2+(3-2)^2)/3) = sqrt(2/3)
    EXPECT_NEAR(2.0 + 2.0 * std::sqrt(2.0 / 3.0), b.upper[2], 1e-12);
    EXPECT_NEAR(2.0 - 2.0 * std::sqrt(2.0 / 3.0), b.lower[2], 1e-12);
    for (int i = 2; i < input.size(); ++i) {
        EXPECT_GE(b.upper[i] + 1e-12, b.mid[i]);
        EXPECT_GE(b.mid[i] + 1e-12, b.lower[i]);
    }
}

TEST(Indicators, KdjBoundsAndRelation)
{
    // n=9 warmup, then k/d must stay in [0,100] and j == 3k-2d exactly.
    QList<double> closes, highs, lows;
    for (int i = 0; i < 40; ++i) {
        const double base = 10.0 + std::sin(i * 0.7) * 3.0;
        closes.append(base + std::sin(i * 0.3));
        highs.append(base + 1.5);
        lows.append(base - 1.5);
    }
    const KdjSeries kd = Indicators::kdj(closes, highs, lows);
    ASSERT_EQ(closes.size(), kd.k.size());
    ASSERT_EQ(closes.size(), kd.d.size());
    ASSERT_EQ(closes.size(), kd.j.size());
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(isNan(kd.k[i]));
        EXPECT_TRUE(isNan(kd.d[i]));
        EXPECT_TRUE(isNan(kd.j[i]));
    }
    for (int i = 8; i < 40; ++i) {
        EXPECT_GE(kd.k[i], 0.0);
        EXPECT_LE(kd.k[i], 100.0);
        EXPECT_GE(kd.d[i], 0.0);
        EXPECT_LE(kd.d[i], 100.0);
        EXPECT_DOUBLE_EQ(3.0 * kd.k[i] - 2.0 * kd.d[i], kd.j[i]);
    }
}

TEST(Indicators, AlignmentInvariants)
{
    // Every function: output size == input size, NaN prefix of the documented
    // length, and no NaN after the warmup.
    QList<double> v;
    for (int i = 0; i < 30; ++i)
        v.append(10.0 + std::sin(i * 0.4) * 5.0);

    const auto checkPrefix = [&](const QVector<double> &out, int prefix) {
        EXPECT_EQ(v.size(), out.size());
        for (int i = 0; i < prefix; ++i)
            EXPECT_TRUE(isNan(out[i])) << "index " << i;
        for (int i = prefix; i < v.size(); ++i)
            EXPECT_FALSE(isNan(out[i])) << "index " << i;
    };

    checkPrefix(Indicators::sma(v, 5), 4);
    checkPrefix(Indicators::ema(v, 5), 4);
    const MacdSeries m = Indicators::macd(v);
    EXPECT_EQ(v.size(), m.diff.size());
    EXPECT_EQ(v.size(), m.dea.size());
    EXPECT_EQ(v.size(), m.hist.size());
    // diff is valid from slow-1; dea seeds at the first valid diff.
    for (int i = 0; i < 25; ++i)
        EXPECT_TRUE(isNan(m.diff[i])) << "diff index " << i;
    for (int i = 25; i < v.size(); ++i)
        EXPECT_FALSE(isNan(m.diff[i])) << "diff index " << i;
    for (int i = 0; i < 25; ++i)
        EXPECT_TRUE(isNan(m.dea[i])) << "dea index " << i;
    for (int i = 25; i < v.size(); ++i)
        EXPECT_FALSE(isNan(m.dea[i])) << "dea index " << i;
    for (int i = 0; i < 25; ++i)
        EXPECT_TRUE(isNan(m.hist[i])) << "hist index " << i;
    for (int i = 25; i < v.size(); ++i)
        EXPECT_FALSE(isNan(m.hist[i])) << "hist index " << i;
    checkPrefix(Indicators::rsi(v, 14), 14);
    checkPrefix(Indicators::boll(v, 20, 2.0).mid, 19);
    checkPrefix(Indicators::boll(v, 20, 2.0).upper, 19);
    // highs/lows offset so the hhv-llv range is never zero (a degenerate
    // high==low window would make RSV 0/0 == NaN forever).
    QList<double> highs, lows;
    for (double x : v) {
        highs.append(x + 1.0);
        lows.append(x - 1.0);
    }
    checkPrefix(Indicators::kdj(v, highs, lows, 9).k, 8);
}
