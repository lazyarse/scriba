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

#include "MarkdownParser.h"
#include "Preferences.h"
#include "Typography.h"

#include <QSettings>
#include <QFile>

using Option = Typography::Option;

namespace {

QString apply(QStringView text, Typography::Options opts)
{
    Typography::State state;
    return Typography::apply(text, opts, state);
}

QString typo(Typography::Options opts, QStringView text)
{
    return apply(text, opts);
}

} // namespace

TEST(TypographyQuotes, DoubleQuotesOpenClose)
{
    EXPECT_EQ(typo(Option::Quotes, u"\"hello\""), QStringLiteral("\u201Chello\u201D"));
    EXPECT_EQ(typo(Option::Quotes, u"he said \"hi\""), QStringLiteral("he said \u201Chi\u201D"));
    EXPECT_EQ(typo(Option::Quotes, u"\"5\""), QStringLiteral("\u201C5\u201D"));
}

TEST(TypographyQuotes, DoubleQuoteClosingAfterMath)
{
    // The closing quote after $...$ sees the math's last content char ('x').
    Typography::State state;
    QString out = Typography::apply(u"\"$x$\"", Option::Quotes, state);
    EXPECT_EQ(out, QStringLiteral("\u201C$x$\u201D"));
}

TEST(TypographyQuotes, ApostrophesAndElisions)
{
    EXPECT_EQ(typo(Option::Quotes, u"don't"), QStringLiteral("don\u2019t"));
    EXPECT_EQ(typo(Option::Quotes, u"O'Brien"), QStringLiteral("O\u2019Brien"));
    EXPECT_EQ(typo(Option::Quotes, u"'tis the season"), QStringLiteral("\u2019tis the season"));
    EXPECT_EQ(typo(Option::Quotes, u"'Twas the night"), QStringLiteral("\u2019Twas the night"));
    EXPECT_EQ(typo(Option::Quotes, u"'80s music"), QStringLiteral("\u201980s music"));
    EXPECT_EQ(typo(Option::Quotes, u"rock 'n' roll"), QStringLiteral("rock \u2019n\u2019 roll"));
    EXPECT_EQ(typo(Option::Quotes, u"'hello'"), QStringLiteral("\u2018hello\u2019"));
}

TEST(TypographyQuotes, ClosingQuoteAtRunStart)
{
    Typography::State state;
    state.lastChar = QLatin1Char('e');
    QString out = Typography::apply(u"\" and\"", Option::Quotes, state);
    EXPECT_EQ(out, QStringLiteral("\u201D and\u201D"));
}

TEST(TypographyDashes, EmEnAndHyphen)
{
    EXPECT_EQ(typo(Option::Dashes, u"a-b"), QStringLiteral("a\u2010b"));
    EXPECT_EQ(typo(Option::Dashes, u"a--b"), QStringLiteral("a\u2013b"));
    EXPECT_EQ(typo(Option::Dashes, u"a---b"), QStringLiteral("a\u2014b"));
    EXPECT_EQ(typo(Option::Dashes, u"a-- --b"), QStringLiteral("a\u2013 \u2013b"));
}

TEST(TypographyEllipsis, ThreeDots)
{
    EXPECT_EQ(typo(Option::Ellipsis, u"wait..."), QStringLiteral("wait\u2026"));
    EXPECT_EQ(typo(Option::Ellipsis, u"a..b"), QStringLiteral("a..b"));
}

TEST(TypographyMultiplication, DigitTimesDigit)
{
    EXPECT_EQ(typo(Option::Multiplication, u"4x4"), QStringLiteral("4\u00D74"));
    EXPECT_EQ(typo(Option::Multiplication, u"4 x 4"), QStringLiteral("4\u00D74"));
    EXPECT_EQ(typo(Option::Multiplication, u"4  x  4"), QStringLiteral("4\u00D74"));
    EXPECT_EQ(typo(Option::Multiplication, u"mix"), QStringLiteral("mix"));
    EXPECT_EQ(typo(Option::Multiplication, u"6x"), QStringLiteral("6x"));
    EXPECT_EQ(typo(Option::Multiplication, u"x2"), QStringLiteral("x2"));
}

TEST(TypographyDegrees, NumberOCelsius)
{
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"90oF"), QStringLiteral("90\u00B0F"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"22 o C"), QStringLiteral("22\u00B0C"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"90oC"), QStringLiteral("90\u00B0C"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"fox"), QStringLiteral("fox"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"2 o'clock"), QStringLiteral("2 o'clock"));
}

TEST(TypographyFractions, SimpleFractions)
{
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"1/2"), QStringLiteral("\u00BD"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"3/4"), QStringLiteral("\u00BE"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"1/7"), QStringLiteral("\u2150"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"7/8"), QStringLiteral("\u215E"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"11/2"), QStringLiteral("11/2"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"1/22"), QStringLiteral("1/22"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"1 / 2"), QStringLiteral("1 / 2"));
}

TEST(TypographyPrimes, FeetAndInches)
{
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"5'10"), QStringLiteral("5\u203210"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"10''"), QStringLiteral("10\u2033"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"5'"), QStringLiteral("5\u2032"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"5's"), QStringLiteral("5's"));
    EXPECT_EQ(typo(Option::DegreeFractionPrime, u"x'"), QStringLiteral("x'"));
}

TEST(TypographyNbsp, SingleLetterWords)
{
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"a bird"), QStringLiteral("a\u00A0bird"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"I am"), QStringLiteral("I\u00A0am"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"aardvark"), QStringLiteral("aardvark"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"ba c"), QStringLiteral("ba c"));
}

TEST(TypographyNbsp, NumberAndUnit)
{
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"10 kg"), QStringLiteral("10\u00A0kg"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"10 %"), QStringLiteral("10\u00A0%"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"10 kilograms"), QStringLiteral("10 kilograms"));
    EXPECT_EQ(typo(Option::NonBreakingSpace, u"x10 kg"), QStringLiteral("x10 kg"));
}

TEST(TypographySymbols, Conversions)
{
    EXPECT_EQ(typo(Option::Symbols, u"(c) 2026"), QStringLiteral("\u00A9 2026"));
    EXPECT_EQ(typo(Option::Symbols, u"(r)"), QStringLiteral("\u00AE"));
    EXPECT_EQ(typo(Option::Symbols, u"Acme (tm)"), QStringLiteral("Acme \u2122"));
    EXPECT_EQ(typo(Option::Symbols, u"(p)"), QStringLiteral("\u2117"));
    EXPECT_EQ(typo(Option::Symbols, u"(sm)"), QStringLiteral("\u2120"));
    EXPECT_EQ(typo(Option::Symbols, u"start (c) end"), QStringLiteral("start \u00A9 end"));
}

TEST(TypographySymbols, NonMatches)
{
    EXPECT_EQ(typo(Option::Symbols, u"func(c)"), QStringLiteral("func(c)"));
    EXPECT_EQ(typo(Option::Symbols, u"(cl)"), QStringLiteral("(cl)"));
    EXPECT_EQ(typo(Option::Symbols, u"(ccc)"), QStringLiteral("(ccc)"));
    EXPECT_EQ(typo(Option::Symbols, u"(C)"), QStringLiteral("(C)"));
    EXPECT_EQ(typo(Option::Symbols, u"tm)"), QStringLiteral("tm)"));
    EXPECT_EQ(typo(Option::Symbols, u"(t)"), QStringLiteral("(t)"));
    EXPECT_EQ(typo(Option::Symbols, u"(c)ash"), QStringLiteral("(c)ash"));
    EXPECT_EQ(typo(Option::Symbols, u"'(c)'"), QStringLiteral("'\u00A9'"));
}

TEST(TypographyArrows, Conversions)
{
    EXPECT_EQ(typo(Option::Arrows, u"a -> b"), QStringLiteral("a \u2192 b"));
    EXPECT_EQ(typo(Option::Arrows, u"a<-b"), QStringLiteral("a\u2190b"));
    EXPECT_EQ(typo(Option::Arrows, u"a <-> b"), QStringLiteral("a \u2194 b"));
    EXPECT_EQ(typo(Option::Arrows, u"a => b"), QStringLiteral("a \u21D2 b"));
    EXPECT_EQ(typo(Option::Arrows, u"a<=b"), QStringLiteral("a\u2264b"));
    EXPECT_EQ(typo(Option::Arrows, u"a>=b"), QStringLiteral("a\u2265b"));
    EXPECT_EQ(typo(Option::Arrows, u"a!=b"), QStringLiteral("a\u2260b"));
    EXPECT_EQ(typo(Option::Arrows, u"+-"), QStringLiteral("\u00B1"));
    EXPECT_EQ(typo(Option::Arrows, u"3 +- 1"), QStringLiteral("3 \u00B1 1"));
}

TEST(TypographyArrows, NonMatches)
{
    EXPECT_EQ(typo(Option::Arrows, u"a-b"), QStringLiteral("a-b"));
    EXPECT_EQ(typo(Option::Arrows, u"a-b>"), QStringLiteral("a-b>"));
    EXPECT_EQ(typo(Option::Arrows, u"x < y"), QStringLiteral("x < y"));
    EXPECT_EQ(typo(Option::Arrows, u"==>"), QStringLiteral("=\u21D2"));
    EXPECT_EQ(typo(Option::Arrows, u"a=b"), QStringLiteral("a=b"));
    EXPECT_EQ(typo(Option::Arrows, u"->x"), QStringLiteral("\u2192x"));
}

TEST(TypographyArrows, BeatsDashes)
{
    EXPECT_EQ(typo(Option::Arrows | Option::Dashes, u"a->b"), QStringLiteral("a\u2192b"));
    EXPECT_EQ(typo(Option::Arrows | Option::Dashes, u"a--b"), QStringLiteral("a\u2013b"));
    EXPECT_EQ(typo(Option::Arrows | Option::Dashes, u"a-b"), QStringLiteral("a\u2010b"));
}

TEST(TypographyMath, SkipsMathRegions)
{
    const auto all = Option::Quotes | Option::Dashes | Option::Ellipsis
                   | Option::Multiplication | Option::DegreeFractionPrime | Option::NonBreakingSpace
                   | Option::Symbols | Option::Arrows;
    EXPECT_EQ(typo(all, u"$1/2$"), QStringLiteral("$1/2$"));
    EXPECT_EQ(typo(all, u"$$x--y$$"), QStringLiteral("$$x--y$$"));
    EXPECT_EQ(typo(all, u"$3x4$"), QStringLiteral("$3x4$"));
    EXPECT_EQ(typo(all, u"$5.00"), QStringLiteral("$5.00"));
    EXPECT_EQ(typo(all, u"$1/2$ and 3x4"), QStringLiteral("$1/2$ and 3\u00D74"));
    EXPECT_EQ(typo(all, u"$(c)$"), QStringLiteral("$(c)$"));
    EXPECT_EQ(typo(all, u"$a -> b$"), QStringLiteral("$a -> b$"));
}

TEST(TypographyMath, KitchensinkMathUntouched)
{
    const auto all = Option::Quotes | Option::Dashes | Option::Ellipsis
                   | Option::Multiplication | Option::DegreeFractionPrime | Option::NonBreakingSpace
                   | Option::Symbols | Option::Arrows;
    EXPECT_EQ(typo(all, u"Inline math: $E = mc^2$, $\\sum_{i=1}^{n} x_i$, $\\int_0^\\infty e^{-x} \\, dx$"),
              QStringLiteral("Inline math: $E = mc^2$, $\\sum_{i=1}^{n} x_i$, $\\int_0^\\infty e^{-x} \\, dx$"));
    EXPECT_EQ(typo(all, u"$\\ce{CH4 + 2O2 -> CO2 + 2H2O}$"),
              QStringLiteral("$\\ce{CH4 + 2O2 -> CO2 + 2H2O}$"));
    EXPECT_EQ(typo(all, u"$\\ce{NaCl(s) ->[\\text{H2O}] Na^+(aq) + Cl^-(aq)}$"),
              QStringLiteral("$\\ce{NaCl(s) ->[\\text{H2O}] Na^+(aq) + Cl^-(aq)}$"));
    EXPECT_EQ(typo(all, u"$\\ce{Fe^{3+}}$, $\\ce{SO4^{2-}}$, $\\ce{Ca^{2+}}$"),
              QStringLiteral("$\\ce{Fe^{3+}}$, $\\ce{SO4^{2-}}$, $\\ce{Ca^{2+}}$"));
}

TEST(TypographyMath, FullKitchensinkMathUntouched)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyQuotes, true);
    settings.setValue(Preferences::TypographyDashes, true);
    settings.setValue(Preferences::TypographyEllipsis, true);
    settings.setValue(Preferences::TypographyMultiplication, true);
    settings.setValue(Preferences::TypographyDegreeFractionPrime, true);
    settings.setValue(Preferences::TypographyNbsp, true);
    settings.setValue(Preferences::TypographySymbols, true);
    settings.setValue(Preferences::TypographyArrows, true);
    settings.sync();

    QFile file(QStringLiteral("docs/kitchensink.md"));
    if (!file.exists())
        file.setFileName(QStringLiteral("../docs/kitchensink.md"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << "open kitchensink.md";
    const QString md = QString::fromUtf8(file.readAll());

    const QString html = MarkdownParser::toHtml(md);

    // Everything between $ delimiters (inline and $$...$$ display) must be
    // verbatim. Toggle inMath at each '$' and collect the content between.
    QString mathContent;
    bool inMath = false;
    for (const QChar ch : html) {
        if (ch == QLatin1Char('$')) {
            inMath = !inMath;
            continue;
        }
        if (inMath)
            mathContent += ch;
    }
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2010")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2013")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2014")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2026")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2018")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2019")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u201D")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2192")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2264")));
    EXPECT_FALSE(mathContent.contains(QStringLiteral("\u2265")));

    for (const auto &key : { Preferences::TypographyQuotes, Preferences::TypographyDashes,
                             Preferences::TypographyEllipsis, Preferences::TypographyMultiplication,
                             Preferences::TypographyDegreeFractionPrime, Preferences::TypographyNbsp,
                             Preferences::TypographySymbols, Preferences::TypographyArrows })
        settings.remove(key);
}

TEST(TypographyOptions, DefaultsOff)
{
    // Clean test config -> optionsFromSettings() enables nothing.
    EXPECT_FALSE(Typography::optionsFromSettings().testFlag(Option::Quotes));
}

TEST(TypographySettings, RoundTrip)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyQuotes, true);
    settings.setValue(Preferences::TypographyDashes, true);
    settings.setValue(Preferences::TypographyNbsp, true);
    settings.setValue(Preferences::TypographySymbols, true);
    settings.setValue(Preferences::TypographyArrows, true);
    settings.sync();

    const auto opts = Typography::optionsFromSettings();
    EXPECT_TRUE(opts.testFlag(Option::Quotes));
    EXPECT_TRUE(opts.testFlag(Option::Dashes));
    EXPECT_TRUE(opts.testFlag(Option::NonBreakingSpace));
    EXPECT_TRUE(opts.testFlag(Option::Symbols));
    EXPECT_TRUE(opts.testFlag(Option::Arrows));
    EXPECT_FALSE(opts.testFlag(Option::Ellipsis));
    EXPECT_FALSE(opts.testFlag(Option::Multiplication));
    EXPECT_FALSE(opts.testFlag(Option::DegreeFractionPrime));

    settings.remove(Preferences::TypographyQuotes);
    settings.remove(Preferences::TypographyDashes);
    settings.remove(Preferences::TypographyNbsp);
    settings.remove(Preferences::TypographySymbols);
    settings.remove(Preferences::TypographyArrows);
}

TEST(TypographyIntegration, RendererConvertsParagraphText)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyQuotes, true);
    settings.setValue(Preferences::TypographyDashes, true);
    settings.sync();

    const QString html = MarkdownParser::toHtml("he said \"hi\" -- ok");
    EXPECT_TRUE(html.contains(QStringLiteral("\u201Chi\u201D")));
    EXPECT_TRUE(html.contains(QStringLiteral("\u2013")));
    EXPECT_FALSE(html.contains("--"));

    settings.remove(Preferences::TypographyQuotes);
    settings.remove(Preferences::TypographyDashes);
}

TEST(TypographyIntegration, CodeBlocksUntouched)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyQuotes, true);
    settings.setValue(Preferences::TypographyDashes, true);
    settings.setValue(Preferences::TypographyEllipsis, true);
    settings.sync();

    const QString html = MarkdownParser::toHtml("`--` and \"quoted\"");
    EXPECT_TRUE(html.contains("<code>--</code>"));
    EXPECT_TRUE(html.contains(QStringLiteral("\u201Cquoted\u201D")));
    EXPECT_FALSE(html.contains("&quot;"));

    const QString fenced = MarkdownParser::toHtml("```\n--\n```");
    EXPECT_FALSE(fenced.contains(QStringLiteral("\u2013")));
    EXPECT_FALSE(fenced.contains(QStringLiteral("\u2014")));
    EXPECT_TRUE(fenced.contains("--"));

    settings.remove(Preferences::TypographyQuotes);
    settings.remove(Preferences::TypographyDashes);
    settings.remove(Preferences::TypographyEllipsis);
}

TEST(TypographyIntegration, ParagraphBoundaryOpeningQuote)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyQuotes, true);
    settings.sync();

    const QString html = MarkdownParser::toHtml("word\n\n\"quoted\"");
    EXPECT_TRUE(html.contains(QStringLiteral("\u201Cquoted\u201D")));

    settings.remove(Preferences::TypographyQuotes);
}

TEST(TypographyIntegration, ArrowsConvertAndSkipMath)
{
    QSettings settings;
    settings.setValue(Preferences::TypographyArrows, true);
    settings.sync();

    const QString html = MarkdownParser::toHtml("x -> y and a<=b");
    EXPECT_TRUE(html.contains(QStringLiteral("\u2192")));
    EXPECT_TRUE(html.contains(QStringLiteral("\u2264")));
    EXPECT_FALSE(html.contains("->"));

    const QString math = MarkdownParser::toHtml("$\\ce{CH4 + 2O2 -> CO2}$");
    EXPECT_TRUE(math.contains("-&gt;"));

    const QString code = MarkdownParser::toHtml("`a->b`");
    EXPECT_TRUE(code.contains("<code>a-&gt;b</code>") || code.contains("<code>a->b</code>"));

    settings.remove(Preferences::TypographyArrows);
}

TEST(TypographyIntegration, DefaultOffPreservesOldOutput)
{
    const QString html = MarkdownParser::toHtml("he said \"hi\" -- 4x4 ... x -> y");
    EXPECT_TRUE(html.contains("&quot;hi&quot;"));
    EXPECT_TRUE(html.contains("--"));
    EXPECT_TRUE(html.contains("4x4"));
    EXPECT_TRUE(html.contains("-&gt;"));
}
