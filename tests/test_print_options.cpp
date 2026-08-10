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
#include "PrintOptions.h"
#include "Preferences.h"
#include <QSettings>

TEST(PrintOptionsTest, DefaultsMatchDr2) {
    // DR-2: defaults preserve today's output — everything "on" / no splitting.
    PrintOptions::Options o;
    EXPECT_EQ(o.codeSplit, PrintOptions::CodeSplit::NeverSplit);
    EXPECT_TRUE(o.keepTables);
    EXPECT_TRUE(o.keepHeadings);
    EXPECT_TRUE(o.keepFigures);
    EXPECT_TRUE(o.orphanControl);
    EXPECT_TRUE(o.pageMargin.isEmpty());
    EXPECT_TRUE(o.pageSize.isEmpty());
}

TEST(PrintOptionsTest, FromSettingsEmptyYieldsDefaults) {
    PrintOptions::Options o = PrintOptions::fromSettings();
    EXPECT_EQ(o.codeSplit, PrintOptions::CodeSplit::NeverSplit);
    EXPECT_TRUE(o.keepTables);
    EXPECT_TRUE(o.keepHeadings);
    EXPECT_TRUE(o.keepFigures);
    EXPECT_TRUE(o.orphanControl);
    EXPECT_TRUE(o.pageMargin.isEmpty());
    EXPECT_TRUE(o.pageSize.isEmpty());
}

TEST(PrintOptionsTest, SettingsRoundTrip) {
    PrintOptions::Options o;
    o.codeSplit = PrintOptions::CodeSplit::SplitLarge;
    o.keepTables = false;
    o.keepHeadings = false;
    o.keepFigures = false;
    o.orphanControl = false;
    o.pageMargin = "18mm";
    o.pageSize = "200mm 100mm";
    PrintOptions::toSettings(o);

    PrintOptions::Options back = PrintOptions::fromSettings();
    EXPECT_EQ(back.codeSplit, PrintOptions::CodeSplit::SplitLarge);
    EXPECT_FALSE(back.keepTables);
    EXPECT_FALSE(back.keepHeadings);
    EXPECT_FALSE(back.keepFigures);
    EXPECT_FALSE(back.orphanControl);
    EXPECT_EQ(back.pageMargin, "18mm");
    EXPECT_EQ(back.pageSize, "200mm 100mm");
}

TEST(PrintOptionsTest, SettingsCodeSplitRoundTripAllModes) {
    for (auto mode : {PrintOptions::CodeSplit::NeverSplit,
                      PrintOptions::CodeSplit::SplitSmall,
                      PrintOptions::CodeSplit::SplitLarge}) {
        PrintOptions::Options o;
        o.codeSplit = mode;
        PrintOptions::toSettings(o);
        EXPECT_EQ(PrintOptions::fromSettings().codeSplit, mode);
    }
}

TEST(PrintOptionsTest, BuildCssAllDefaultsEmitsNothing) {
    PrintOptions::Options o;
    EXPECT_TRUE(PrintOptions::buildCss(o).isEmpty());
}

TEST(PrintOptionsTest, BuildCssCodeSplitFragments) {
    PrintOptions::Options o;
    o.codeSplit = PrintOptions::CodeSplit::SplitSmall;
    EXPECT_EQ(PrintOptions::buildCss(o),
        "pre.scriba-split-small{break-inside:auto;page-break-inside:auto}");
    o.codeSplit = PrintOptions::CodeSplit::SplitLarge;
    EXPECT_EQ(PrintOptions::buildCss(o),
        "pre.scriba-split-large{break-inside:auto;page-break-inside:auto}");
}

TEST(PrintOptionsTest, BuildCssOffOverrides) {
    PrintOptions::Options o;
    o.keepTables = false;
    EXPECT_EQ(PrintOptions::buildCss(o), "table{break-inside:auto;page-break-inside:auto}");

    o = PrintOptions::Options();
    o.keepHeadings = false;
    EXPECT_EQ(PrintOptions::buildCss(o),
        "h1,h2,h3,h4,h5,h6{break-after:auto;page-break-after:auto}");

    o = PrintOptions::Options();
    o.keepFigures = false;
    EXPECT_EQ(PrintOptions::buildCss(o),
        ".mermaid,.katex-display,.admonition,blockquote,pre{break-inside:auto;page-break-inside:auto}");

    o = PrintOptions::Options();
    o.orphanControl = false;
    EXPECT_EQ(PrintOptions::buildCss(o), "p{orphans:1;widows:1}");
}

TEST(PrintOptionsTest, BuildCssCombinesFragmentsNewlineSeparated) {
    PrintOptions::Options o;
    o.codeSplit = PrintOptions::CodeSplit::SplitSmall;
    o.keepTables = false;
    o.orphanControl = false;
    QString css = PrintOptions::buildCss(o);
    EXPECT_EQ(css,
        "pre.scriba-split-small{break-inside:auto;page-break-inside:auto}\n"
        "table{break-inside:auto;page-break-inside:auto}\n"
        "p{orphans:1;widows:1}");
}

TEST(PrintOptionsTest, PageOverrideEmptyWhenUnset) {
    PrintOptions::Options o;
    EXPECT_TRUE(PrintOptions::buildPageOverrideCss(o).isEmpty());
}

TEST(PrintOptionsTest, PageOverrideMarginOnly) {
    PrintOptions::Options o;
    o.pageMargin = "18mm";
    EXPECT_EQ(PrintOptions::buildPageOverrideCss(o), "@page{margin:18mm}");
}

TEST(PrintOptionsTest, PageOverrideSizeOnly) {
    PrintOptions::Options o;
    o.pageSize = "A4";
    EXPECT_EQ(PrintOptions::buildPageOverrideCss(o), "@page{size:A4}");
}

TEST(PrintOptionsTest, PageOverrideSizeAndMargin) {
    PrintOptions::Options o;
    o.pageSize = "200mm 100mm";
    o.pageMargin = "30mm";
    EXPECT_EQ(PrintOptions::buildPageOverrideCss(o), "@page{size:200mm 100mm;margin:30mm}");
}

TEST(PrintOptionsTest, PageOverrideEmitsSingleAtPageRule) {
    // The override is meant to be appended LAST so it wins the CSS cascade over
    // print-base.css's `@page { margin: 15mm; }` (DR-4). Assert the block is a
    // single @page rule so the "last match" parsing in the Qt fallback (Task
    // 2.3) can find it. The appended-LAST ordering itself is asserted in
    // test_pdf_export (Task 2.1) via buildFullHtml.
    PrintOptions::Options o;
    o.pageMargin = "18mm";
    QString css = PrintOptions::buildPageOverrideCss(o);
    EXPECT_EQ(css.count(QStringLiteral("@page{")), 1);
}
