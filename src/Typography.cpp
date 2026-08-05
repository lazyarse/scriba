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
#include "Preferences.h"

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

} // namespace

QString Typography::apply(QStringView text, Options opts, State &state)
{
    QString out;
    out.reserve(text.size());
    const qsizetype n = text.size();

    // Characters before/after the current position. Within-run access is exact;
    // at run boundaries the caller's carried State provides the last char.
    auto prevImmediate = [&text, &state](qsizetype i) -> QChar {
        if (i > 0)
            return text[i - 1];
        return state.lastChar;
    };
    auto nextImmediate = [&text, n](qsizetype i) -> QChar {
        if (i + 1 < n)
            return text[i + 1];
        return QChar();
    };
    // Next char as seen by the quote rules: math delimiters ($/$$) are skipped
    // so a quote opening directly before "$x$" is recognised as opening.
    auto nextForQuote = [&text, n](qsizetype i) -> QChar {
        qsizetype j = i + 1;
        while (j < n && text[j] == QLatin1Char('$'))
            ++j;
        return j < n ? text[j] : QChar();
    };
    // Last non-space char before the position (within the run, else carried).
    auto prevSignificant = [&text, &state](qsizetype i) -> QChar {
        for (qsizetype j = i - 1; j >= 0; --j) {
            if (!text[j].isSpace())
                return text[j];
        }
        return state.lastChar;
    };
    auto nextSignificant = [&text, n](qsizetype i) -> QChar {
        for (qsizetype j = i + 1; j < n; ++j) {
            if (!text[j].isSpace())
                return text[j];
        }
        return QChar();
    };

    // effectiveLast is like state.lastChar but ignores math delimiters, so a
    // quote that follows $...$ still sees the math's last content character.
    QChar effectiveLast = state.lastChar;
    auto append = [&out, &effectiveLast](QChar c, bool track = true) {
        out.append(c);
        if (track)
            effectiveLast = c;
    };
    // Pop a trailing run of plain spaces we emitted earlier (used when a
    // conversion absorbs surrounding spaces, e.g. "4 x 4" -> "4×4").
    auto popTrailingSpaces = [&out, &effectiveLast, &state]() {
        while (!out.isEmpty() && out.back().isSpace())
            out.chop(1);
        effectiveLast = out.isEmpty() ? state.lastChar : out.back();
    };

    qsizetype i = 0;
    while (i < n) {
        const QChar c = text[i];

        // --- Math regions pass through verbatim. ---
        if (state.inDisplayMath) {
            if (c == QLatin1Char('$') && i + 1 < n && text[i + 1] == QLatin1Char('$')) {
                append(QLatin1Char('$'), false);
                append(QLatin1Char('$'), false);
                state.inDisplayMath = false;
                i += 2;
            } else {
                append(c);
                ++i;
            }
            continue;
        }
        if (state.inMath) {
            if (c == QLatin1Char('$')) {
                append(QLatin1Char('$'), false);
                state.inMath = false;
                ++i;
            } else {
                append(c);
                ++i;
            }
            continue;
        }

        if (c == QLatin1Char('$')) {
            if (i + 1 < n && text[i + 1] == QLatin1Char('$')) {
                append(QLatin1Char('$'), false);
                append(QLatin1Char('$'), false);
                state.inDisplayMath = true;
                i += 2;
            } else if (i + 1 < n && !text[i + 1].isSpace()) {
                // Inline math $...$. md4c may split a span across several text
                // runs (e.g. around \-escapes), so the closing $ need not be in
                // this run. Only a $ followed by a digit with no closing $ in
                // the run is treated as currency ("$5.00") and left alone; any
                // dangling math state is reset at the next block boundary.
                bool hasClosing = false;
                for (qsizetype j = i + 1; j < n; ++j) {
                    if (text[j] == QLatin1Char('$')) {
                        hasClosing = true;
                        break;
                    }
                }
                if (hasClosing || !text[i + 1].isDigit()) {
                    append(QLatin1Char('$'), false);
                    state.inMath = true;
                    ++i;
                } else {
                    append(QLatin1Char('$'));
                    ++i;
                }
            } else {
                append(QLatin1Char('$'));
                ++i;
            }
            continue;
        }

        // --- Ellipsis: ... -> … ---
        if (opts.testFlag(Option::Ellipsis) && c == QLatin1Char('.')
            && i + 2 < n && text[i + 1] == QLatin1Char('.') && text[i + 2] == QLatin1Char('.')) {
            append(QChar(0x2026));
            i += 3;
            continue;
        }

        // --- Dashes: --- -> em dash, -- -> en dash, - -> hyphen ---
        if (opts.testFlag(Option::Dashes) && c == QLatin1Char('-')) {
            if (i + 2 < n && text[i + 1] == QLatin1Char('-') && text[i + 2] == QLatin1Char('-')) {
                append(QChar(0x2014));
                i += 3;
                continue;
            }
            if (i + 1 < n && text[i + 1] == QLatin1Char('-')) {
                append(QChar(0x2013));
                i += 2;
                continue;
            }
            append(QChar(0x2010));
            ++i;
            continue;
        }

        // --- Multiplication: 4x4 / 4 x 4 -> 4×4 ---
        if (opts.testFlag(Option::Multiplication) && c == QLatin1Char('x')) {
            const QChar before = prevSignificant(i);
            const QChar after = nextSignificant(i);
            if (before.isDigit() && after.isDigit()) {
                popTrailingSpaces();
                append(QChar(0x00D7));
                while (i + 1 < n && text[i + 1].isSpace())
                    ++i;
                ++i; // skip past the following digit too
                continue;
            }
        }

        // --- Degrees: 90oF / 22 o C -> 90°F / 22°C ---
        if (opts.testFlag(Option::DegreeFractionPrime) && c == QLatin1Char('o')) {
            const QChar before = prevSignificant(i);
            const QChar after = nextSignificant(i);
            if (before.isDigit() && (after == QLatin1Char('C') || after == QLatin1Char('F'))) {
                popTrailingSpaces();
                append(QChar(0x00B0));
                while (i + 1 < n && text[i + 1].isSpace())
                    ++i;
                ++i; // land on the C/F (the 'o' is consumed)
                continue;
            }
        }

        // --- Fractions: 1/2 -> ½ (only single digits, word boundaries) ---
        if (opts.testFlag(Option::DegreeFractionPrime) && c == QLatin1Char('/')) {
            const QChar num = prevImmediate(i);
            const QChar den = nextImmediate(i);
            if (num.isDigit() && den.isDigit()) {
                const QChar numPrev = i >= 2 ? text[i - 2] : state.lastChar;
                const QChar denNext = i + 2 < n ? text[i + 2] : QChar();
                if (!numPrev.isDigit() && !denNext.isDigit()) {
                    const QChar frac = vulgarFraction(num, den);
                    if (!frac.isNull()) {
                        out.chop(1); // remove the already-emitted numerator
                        append(frac);
                        i += 2; // skip '/' and the denominator
                        continue;
                    }
                }
            }
        }

        // --- Primes: 5'10 -> 5′10, 10'' -> 10″ ---
        if (opts.testFlag(Option::DegreeFractionPrime) && c == QLatin1Char('\'')) {
            const QChar prev = effectiveLast;
            const QChar next = nextImmediate(i);
            if (prev.isDigit()) {
                if (next == QLatin1Char('\'')) {
                    append(QChar(0x2033));
                    i += 2;
                    continue;
                }
                if (next.isDigit() || next.isSpace() || next == QLatin1Char('"') || next.isNull()) {
                    append(QChar(0x2032));
                    ++i;
                    continue;
                }
            }
        }

        // --- Symbols: (c) (r) (tm) (p) (sm) -> © ® ™ ℗ ℠ ---
        if (opts.testFlag(Option::Symbols) && c == QLatin1Char('(')) {
            const QChar prev = prevImmediate(i);
            if (prev.isNull() || !prev.isLetterOrNumber()) {
                const char16_t replacement = [&]() -> char16_t {
                    if (i + 2 < n && text[i + 1] == QLatin1Char('c') && text[i + 2] == QLatin1Char(')'))
                        return 0x00A9; // ©
                    if (i + 2 < n && text[i + 1] == QLatin1Char('r') && text[i + 2] == QLatin1Char(')'))
                        return 0x00AE; // ®
                    if (i + 2 < n && text[i + 1] == QLatin1Char('p') && text[i + 2] == QLatin1Char(')'))
                        return 0x2117; // ℗
                    if (i + 3 < n && text[i + 1] == QLatin1Char('t') && text[i + 2] == QLatin1Char('m')
                        && text[i + 3] == QLatin1Char(')'))
                        return 0x2122; // ™
                    if (i + 3 < n && text[i + 1] == QLatin1Char('s') && text[i + 2] == QLatin1Char('m')
                        && text[i + 3] == QLatin1Char(')'))
                        return 0x2120; // ℠
                    return 0;
                }();
                if (replacement != 0) {
                    const qsizetype consumed = replacement == 0x2122 || replacement == 0x2120 ? 4 : 3;
                    const QChar next = i + consumed < n ? text[i + consumed] : QChar();
                    if (next.isNull() || !next.isLetter()) {
                        append(QChar(replacement));
                        i += consumed;
                        continue;
                    }
                }
            }
        }

        // --- Curly double quotes: " -> " / " ---
        if (opts.testFlag(Option::Quotes) && c == QLatin1Char('"')) {
            const QChar next = nextForQuote(i);
            const bool opening = next.isNull() ? !effectiveLast.isLetterOrNumber()
                                               : next.isLetterOrNumber();
            append(opening ? QChar(0x201C) : QChar(0x201D));
            ++i;
            continue;
        }

        // --- Curly single quotes / apostrophes: ' -> ' / ' ---
        if (opts.testFlag(Option::Quotes) && c == QLatin1Char('\'')) {
            const QChar prev = effectiveLast;
            const QChar next = nextForQuote(i);
            if (prev.isLetterOrNumber()) {
                append(QChar(0x2019)); // closing / apostrophe (don't, dogs')
                ++i;
                continue;
            }
            if (next.isDigit()) {
                append(QChar(0x2019)); // decades: '80s
                ++i;
                continue;
            }
            if (!next.isNull() && next.isLetter()) {
                QString word;
                for (qsizetype j = i + 1; j < n && text[j].isLetter(); ++j)
                    word += text[j];
                const bool elision = elisions().contains(word.toLower());
                append(elision ? QChar(0x2019) : QChar(0x2018));
                ++i;
                continue;
            }
            append(QChar(0x2019)); // lone / closing
            ++i;
            continue;
        }

        // --- Non-breaking spaces: a word, 10 kg, 10 % ---
        if (opts.testFlag(Option::NonBreakingSpace) && c.isSpace()) {
            const QChar prev = prevImmediate(i);
            const QChar next = nextImmediate(i);

            if (isSingleLetterWord(prev) && !next.isNull() && next.isLetter()) {
                const QChar beforeLetter = i >= 2 ? text[i - 2] : state.lastChar;
                if (!beforeLetter.isLetter()) {
                    append(QChar(0x00A0));
                    ++i;
                    continue;
                }
            }

            if (prev.isDigit()) {
                qsizetype j = i - 1;
                while (j >= 0 && text[j].isDigit())
                    --j;
                const QChar beforeNumber = j >= 0 ? text[j] : state.lastChar;
                if (!beforeNumber.isLetterOrNumber()) {
                    if (!next.isNull() && next.isLetter()) {
                        QString unit;
                        qsizetype k = i + 1;
                        while (k < n && text[k].isLetter()) {
                            unit += text[k];
                            ++k;
                        }
                        if (units().contains(unit)) {
                            append(QChar(0x00A0));
                            ++i;
                            continue;
                        }
                    }
                    if (next == QLatin1Char('%')) {
                        append(QChar(0x00A0));
                        ++i;
                        continue;
                    }
                }
            }
        }

        append(c);
        ++i;
    }

    state.lastChar = effectiveLast;
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
    return opts;
}
