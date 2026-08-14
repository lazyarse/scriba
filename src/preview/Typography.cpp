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
#include "Typography.h"
#include "prefs/Preferences.h"

#include <QSettings>
#include <QStringList>

#include <array>

namespace {

// Single-letter words after which a regular space becomes non-breaking.
bool isSingleLetterWord(QChar c)
{
    return c == QLatin1Char('a') || c == QLatin1Char('A') || c == QLatin1Char('I');
}

// Common word-initial apostrophes ("'tis", "'80s") that are elisions, not
// opening single quotes.
const QStringList &elisions()
{
    static const QStringList list = {
        QStringLiteral("tis"), QStringLiteral("twas"), QStringLiteral("twere"),
        QStringLiteral("em"), QStringLiteral("cause"), QStringLiteral("round"),
        QStringLiteral("bout"), QStringLiteral("neath"), QStringLiteral("gainst"),
        QStringLiteral("til"), QStringLiteral("till"), QStringLiteral("twixt"),
        QStringLiteral("n"),
    };
    return list;
}

// Measurement/abbreviation units that take a non-breaking space after a number.
const QStringList &units()
{
    static const QStringList list = {
        QStringLiteral("kg"), QStringLiteral("g"), QStringLiteral("mg"),
        QStringLiteral("t"), QStringLiteral("km"), QStringLiteral("m"),
        QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("nm"),
        QStringLiteral("ml"), QStringLiteral("l"), QStringLiteral("s"),
        QStringLiteral("ms"), QStringLiteral("min"), QStringLiteral("h"),
        QStringLiteral("Hz"), QStringLiteral("kHz"), QStringLiteral("MHz"),
        QStringLiteral("GHz"), QStringLiteral("TB"), QStringLiteral("GB"),
        QStringLiteral("MB"), QStringLiteral("KB"), QStringLiteral("kbps"),
        QStringLiteral("Mbps"), QStringLiteral("Gbps"), QStringLiteral("W"),
        QStringLiteral("kW"), QStringLiteral("MW"), QStringLiteral("V"),
        QStringLiteral("mV"), QStringLiteral("A"), QStringLiteral("mA"),
    };
    return list;
}

// Numerator/denominator pairs -> Unicode vulgar fractions.
QChar vulgarFraction(QChar numerator, QChar denominator)
{
    struct Pair { char num; char den; char16_t frac; };
    static constexpr std::array<Pair, 16> table = {{
        {'1', '2', 0x00BD}, {'1', '3', 0x2153}, {'2', '3', 0x2154},
        {'1', '4', 0x00BC}, {'3', '4', 0x00BE}, {'1', '5', 0x2155},
        {'2', '5', 0x2156}, {'3', '5', 0x2157}, {'4', '5', 0x2158},
        {'1', '6', 0x2159}, {'5', '6', 0x215A}, {'1', '7', 0x2150},
        {'1', '8', 0x215B}, {'3', '8', 0x215C}, {'5', '8', 0x215D}, {'7', '8', 0x215E},
    }};
    const char n = numerator.toLatin1();
    const char d = denominator.toLatin1();
    for (const auto &p : table) {
        if (p.num == n && p.den == d)
            return QChar(p.frac);
    }
    return QChar();
}

// Mutable scratch shared by the per-replacement helpers below: the output
// buffer, the input view, the caller's carried State, the enabled options and
// "effectiveLast" (state.lastChar minus math delimiters) plus the look-ahead /
// append helpers every replacement class uses.
struct FormatContext {
    QString &out;
    QStringView text;
    qsizetype n;
    Typography::State &state;
    Typography::Options opts;
    QChar effectiveLast;

    // Characters before/after the current position. Within-run access is exact;
    // at run boundaries the caller's carried State provides the last char.
    QChar prevImmediate(qsizetype i) const
    {
        if (i > 0)
            return text[i - 1];
        return state.lastChar;
    }
    QChar nextImmediate(qsizetype i) const
    {
        if (i + 1 < n)
            return text[i + 1];
        return QChar();
    }
    // Next char as seen by the quote rules: math delimiters ($/$$) are skipped
    // so a quote opening directly before "$x$" is recognised as opening.
    QChar nextForQuote(qsizetype i) const
    {
        qsizetype j = i + 1;
        while (j < n && text[j] == QLatin1Char('$'))
            ++j;
        return j < n ? text[j] : QChar();
    }
    // Last non-space char before the position (within the run, else carried).
    QChar prevSignificant(qsizetype i) const
    {
        for (qsizetype j = i - 1; j >= 0; --j) {
            if (!text[j].isSpace())
                return text[j];
        }
        return state.lastChar;
    }
    QChar nextSignificant(qsizetype i) const
    {
        for (qsizetype j = i + 1; j < n; ++j) {
            if (!text[j].isSpace())
                return text[j];
        }
        return QChar();
    }

    // effectiveLast is like state.lastChar but ignores math delimiters, so a
    // quote that follows $...$ still sees the math's last content character.
    void append(QChar c, bool track = true)
    {
        out.append(c);
        if (track)
            effectiveLast = c;
    }
    // Pop a trailing run of plain spaces we emitted earlier (used when a
    // conversion absorbs surrounding spaces, e.g. "4 x 4" -> "4×4").
    void popTrailingSpaces()
    {
        while (!out.isEmpty() && out.back().isSpace())
            out.chop(1);
        effectiveLast = out.isEmpty() ? state.lastChar : out.back();
    }
};

// Each helper below handles one replacement class. It returns true when it
// consumed input (advancing i); false when the caller must try the next class.
// The caller tries them in the same order as the original if-chain, so the
// replacement priority is unchanged.

static bool tryMathRegion(FormatContext &ctx, qsizetype &i)
{
    // --- Math regions pass through verbatim. ---
    const QChar c = ctx.text[i];
    if (ctx.state.inDisplayMath) {
        if (c == QLatin1Char('$') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('$')) {
            ctx.append(QLatin1Char('$'), false);
            ctx.append(QLatin1Char('$'), false);
            ctx.state.inDisplayMath = false;
            i += 2;
        } else {
            ctx.append(c);
            ++i;
        }
        return true;
    }
    if (ctx.state.inMath) {
        if (c == QLatin1Char('$')) {
            ctx.append(QLatin1Char('$'), false);
            ctx.state.inMath = false;
            ++i;
        } else {
            ctx.append(c);
            ++i;
        }
        return true;
    }

    if (c == QLatin1Char('$')) {
        if (i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('$')) {
            ctx.append(QLatin1Char('$'), false);
            ctx.append(QLatin1Char('$'), false);
            ctx.state.inDisplayMath = true;
            i += 2;
        } else if (i + 1 < ctx.n && !ctx.text[i + 1].isSpace()) {
            // Inline math $...$. md4c may split a span across several text
            // runs (e.g. around \-escapes), so the closing $ need not be in
            // this run. Only a $ followed by a digit with no closing $ in
            // the run is treated as currency ("$5.00") and left alone; any
            // dangling math state is reset at the next block boundary.
            bool hasClosing = false;
            for (qsizetype j = i + 1; j < ctx.n; ++j) {
                if (ctx.text[j] == QLatin1Char('$')) {
                    hasClosing = true;
                    break;
                }
            }
            if (hasClosing || !ctx.text[i + 1].isDigit()) {
                ctx.append(QLatin1Char('$'), false);
                ctx.state.inMath = true;
                ++i;
            } else {
                ctx.append(QLatin1Char('$'));
                ++i;
            }
        } else {
            ctx.append(QLatin1Char('$'));
            ++i;
        }
        return true;
    }
    return false;
}

static bool tryEllipsis(FormatContext &ctx, qsizetype &i)
{
    // --- Ellipsis: ... -> … ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Ellipsis) || c != QLatin1Char('.'))
        return false;
    if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('.')
        && ctx.text[i + 2] == QLatin1Char('.')) {
        ctx.append(QChar(0x2026));
        i += 3;
        return true;
    }
    return false;
}

static bool tryArrows(FormatContext &ctx, qsizetype &i)
{
    // --- Arrows and comparison signs: -> <- <-> => <= >= != +- ---
    if (!ctx.opts.testFlag(Typography::Option::Arrows))
        return false;
    const QChar c = ctx.text[i];
    if (c == QLatin1Char('-') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('>')) {
        ctx.append(QChar(0x2192)); // →
        i += 2;
        return true;
    }
    if (c == QLatin1Char('<')) {
        if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('-') && ctx.text[i + 2] == QLatin1Char('>')) {
            ctx.append(QChar(0x2194)); // ↔
            i += 3;
            return true;
        }
        if (i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('-')) {
            ctx.append(QChar(0x2190)); // ←
            i += 2;
            return true;
        }
        if (i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('=')) {
            ctx.append(QChar(0x2264)); // ≤
            i += 2;
            return true;
        }
    }
    if (c == QLatin1Char('=') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('>')) {
        ctx.append(QChar(0x21D2)); // ⇒
        i += 2;
        return true;
    }
    if (c == QLatin1Char('>') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('=')) {
        ctx.append(QChar(0x2265)); // ≥
        i += 2;
        return true;
    }
    if (c == QLatin1Char('!') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('=')) {
        ctx.append(QChar(0x2260)); // ≠
        i += 2;
        return true;
    }
    if (c == QLatin1Char('+') && i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('-')) {
        ctx.append(QChar(0x00B1)); // ±
        i += 2;
        return true;
    }
    return false;
}

static bool tryDashes(FormatContext &ctx, qsizetype &i)
{
    // --- Dashes: --- -> em dash, -- -> en dash, - -> hyphen ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Dashes) || c != QLatin1Char('-'))
        return false;
    if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('-') && ctx.text[i + 2] == QLatin1Char('-')) {
        ctx.append(QChar(0x2014));
        i += 3;
        return true;
    }
    if (i + 1 < ctx.n && ctx.text[i + 1] == QLatin1Char('-')) {
        ctx.append(QChar(0x2013));
        i += 2;
        return true;
    }
    ctx.append(QChar(0x2010));
    ++i;
    return true;
}

static bool tryMultiplication(FormatContext &ctx, qsizetype &i)
{
    // --- Multiplication: 4x4 / 4 x 4 -> 4×4 ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Multiplication) || c != QLatin1Char('x'))
        return false;
    const QChar before = ctx.prevSignificant(i);
    const QChar after = ctx.nextSignificant(i);
    if (!(before.isDigit() && after.isDigit()))
        return false;
    ctx.popTrailingSpaces();
    ctx.append(QChar(0x00D7));
    while (i + 1 < ctx.n && ctx.text[i + 1].isSpace())
        ++i;
    ++i; // skip past the following digit too
    return true;
}

static bool tryDegrees(FormatContext &ctx, qsizetype &i)
{
    // --- Degrees: 90oF / 22 o C -> 90°F / 22°C ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::DegreeFractionPrime) || c != QLatin1Char('o'))
        return false;
    const QChar before = ctx.prevSignificant(i);
    const QChar after = ctx.nextSignificant(i);
    if (!(before.isDigit() && (after == QLatin1Char('C') || after == QLatin1Char('F'))))
        return false;
    ctx.popTrailingSpaces();
    ctx.append(QChar(0x00B0));
    while (i + 1 < ctx.n && ctx.text[i + 1].isSpace())
        ++i;
    ++i; // land on the C/F (the 'o' is consumed)
    return true;
}

static bool tryFraction(FormatContext &ctx, qsizetype &i)
{
    // --- Fractions: 1/2 -> ½ (only single digits, word boundaries) ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::DegreeFractionPrime) || c != QLatin1Char('/'))
        return false;
    const QChar num = ctx.prevImmediate(i);
    const QChar den = ctx.nextImmediate(i);
    if (!(num.isDigit() && den.isDigit()))
        return false;
    const QChar numPrev = i >= 2 ? ctx.text[i - 2] : ctx.state.lastChar;
    const QChar denNext = i + 2 < ctx.n ? ctx.text[i + 2] : QChar();
    if (numPrev.isDigit() || denNext.isDigit())
        return false;
    const QChar frac = vulgarFraction(num, den);
    if (frac.isNull())
        return false;
    ctx.out.chop(1); // remove the already-emitted numerator
    ctx.append(frac);
    i += 2; // skip '/' and the denominator
    return true;
}

static bool tryPrimes(FormatContext &ctx, qsizetype &i)
{
    // --- Primes: 5'10 -> 5′10, 10'' -> 10″ ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::DegreeFractionPrime) || c != QLatin1Char('\''))
        return false;
    const QChar prev = ctx.effectiveLast;
    const QChar next = ctx.nextImmediate(i);
    if (!prev.isDigit())
        return false;
    if (next == QLatin1Char('\'')) {
        ctx.append(QChar(0x2033));
        i += 2;
        return true;
    }
    if (next.isDigit() || next.isSpace() || next == QLatin1Char('"') || next.isNull()) {
        ctx.append(QChar(0x2032));
        ++i;
        return true;
    }
    return false;
}

static bool trySymbols(FormatContext &ctx, qsizetype &i)
{
    // --- Symbols: (c) (r) (tm) (p) (sm) -> © ® ™ ℗ ℠ ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Symbols) || c != QLatin1Char('('))
        return false;
    const QChar prev = ctx.prevImmediate(i);
    if (prev.isNull() || !prev.isLetterOrNumber()) {
        const char16_t replacement = [&]() -> char16_t {
            if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('c') && ctx.text[i + 2] == QLatin1Char(')'))
                return 0x00A9; // ©
            if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('r') && ctx.text[i + 2] == QLatin1Char(')'))
                return 0x00AE; // ®
            if (i + 2 < ctx.n && ctx.text[i + 1] == QLatin1Char('p') && ctx.text[i + 2] == QLatin1Char(')'))
                return 0x2117; // ℗
            if (i + 3 < ctx.n && ctx.text[i + 1] == QLatin1Char('t') && ctx.text[i + 2] == QLatin1Char('m')
                && ctx.text[i + 3] == QLatin1Char(')'))
                return 0x2122; // ™
            if (i + 3 < ctx.n && ctx.text[i + 1] == QLatin1Char('s') && ctx.text[i + 2] == QLatin1Char('m')
                && ctx.text[i + 3] == QLatin1Char(')'))
                return 0x2120; // ℠
            return 0;
        }();
        if (replacement != 0) {
            const qsizetype consumed = replacement == 0x2122 || replacement == 0x2120 ? 4 : 3;
            const QChar next = i + consumed < ctx.n ? ctx.text[i + consumed] : QChar();
            if (next.isNull() || !next.isLetter()) {
                ctx.append(QChar(replacement));
                i += consumed;
                return true;
            }
        }
    }
    return false;
}

static bool tryCurlyDoubleQuote(FormatContext &ctx, qsizetype &i)
{
    // --- Curly double quotes: " -> " / " ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Quotes) || c != QLatin1Char('"'))
        return false;
    const QChar next = ctx.nextForQuote(i);
    const bool opening = next.isNull() ? !ctx.effectiveLast.isLetterOrNumber()
                                       : next.isLetterOrNumber();
    ctx.append(opening ? QChar(0x201C) : QChar(0x201D));
    ++i;
    return true;
}

static bool tryCurlySingleQuote(FormatContext &ctx, qsizetype &i)
{
    // --- Curly single quotes / apostrophes: ' -> ' / ' ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::Quotes) || c != QLatin1Char('\''))
        return false;
    const QChar prev = ctx.effectiveLast;
    const QChar next = ctx.nextForQuote(i);
    if (prev.isLetterOrNumber()) {
        ctx.append(QChar(0x2019)); // closing / apostrophe (don't, dogs')
        ++i;
        return true;
    }
    if (next.isDigit()) {
        ctx.append(QChar(0x2019)); // decades: '80s
        ++i;
        return true;
    }
    if (!next.isNull() && next.isLetter()) {
        QString word;
        for (qsizetype j = i + 1; j < ctx.n && ctx.text[j].isLetter(); ++j)
            word += ctx.text[j];
        const bool elision = elisions().contains(word.toLower());
        ctx.append(elision ? QChar(0x2019) : QChar(0x2018));
        ++i;
        return true;
    }
    ctx.append(QChar(0x2019)); // lone / closing
    ++i;
    return true;
}

static bool tryNonBreakingSpace(FormatContext &ctx, qsizetype &i)
{
    // --- Non-breaking spaces: a word, 10 kg, 10 % ---
    const QChar c = ctx.text[i];
    if (!ctx.opts.testFlag(Typography::Option::NonBreakingSpace) || !c.isSpace())
        return false;
    const QChar prev = ctx.prevImmediate(i);
    const QChar next = ctx.nextImmediate(i);

    if (isSingleLetterWord(prev) && !next.isNull() && next.isLetter()) {
        const QChar beforeLetter = i >= 2 ? ctx.text[i - 2] : ctx.state.lastChar;
        if (!beforeLetter.isLetter()) {
            ctx.append(QChar(0x00A0));
            ++i;
            return true;
        }
    }

    if (prev.isDigit()) {
        qsizetype j = i - 1;
        while (j >= 0 && ctx.text[j].isDigit())
            --j;
        const QChar beforeNumber = j >= 0 ? ctx.text[j] : ctx.state.lastChar;
        if (!beforeNumber.isLetterOrNumber()) {
            if (!next.isNull() && next.isLetter()) {
                QString unit;
                qsizetype k = i + 1;
                while (k < ctx.n && ctx.text[k].isLetter()) {
                    unit += ctx.text[k];
                    ++k;
                }
                if (units().contains(unit)) {
                    ctx.append(QChar(0x00A0));
                    ++i;
                    return true;
                }
            }
            if (next == QLatin1Char('%')) {
                ctx.append(QChar(0x00A0));
                ++i;
                return true;
            }
        }
    }
    return false;
}

} // namespace

QString Typography::apply(QStringView text, Options opts, State &state)
{
    QString out;
    out.reserve(text.size());
    const qsizetype n = text.size();

    FormatContext ctx{out, text, n, state, opts, state.lastChar};

    qsizetype i = 0;
    while (i < n) {
        if (tryMathRegion(ctx, i))
            continue;
        if (tryEllipsis(ctx, i))
            continue;
        if (tryArrows(ctx, i))
            continue;
        if (tryDashes(ctx, i))
            continue;
        if (tryMultiplication(ctx, i))
            continue;
        if (tryDegrees(ctx, i))
            continue;
        if (tryFraction(ctx, i))
            continue;
        if (tryPrimes(ctx, i))
            continue;
        if (trySymbols(ctx, i))
            continue;
        if (tryCurlyDoubleQuote(ctx, i))
            continue;
        if (tryCurlySingleQuote(ctx, i))
            continue;
        if (tryNonBreakingSpace(ctx, i))
            continue;

        ctx.append(text[i]);
        ++i;
    }

    state.lastChar = ctx.effectiveLast;
    return out;
}

Typography::Options Typography::optionsFromSettings()
{
    QSettings settings;
    Options opts;
    if (settings.value(Preferences::TypographyQuotes, false).toBool())
        opts |= Option::Quotes;
    if (settings.value(Preferences::TypographyDashes, false).toBool())
        opts |= Option::Dashes;
    if (settings.value(Preferences::TypographyEllipsis, false).toBool())
        opts |= Option::Ellipsis;
    if (settings.value(Preferences::TypographyMultiplication, false).toBool())
        opts |= Option::Multiplication;
    if (settings.value(Preferences::TypographyDegreeFractionPrime, false).toBool())
        opts |= Option::DegreeFractionPrime;
    if (settings.value(Preferences::TypographyNbsp, false).toBool())
        opts |= Option::NonBreakingSpace;
    if (settings.value(Preferences::TypographySymbols, false).toBool())
        opts |= Option::Symbols;
    if (settings.value(Preferences::TypographyArrows, false).toBool())
        opts |= Option::Arrows;
    return opts;
}
