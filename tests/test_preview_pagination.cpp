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
#include <QApplication>
#include <QTest>
#include <QWebEngineView>

#include "preview/PreviewPagination.h"
#include "preview/PrintOptions.h"
#include "TestConfig.h"

// ---------- Pure string tests (no WebEngine) ----------

TEST(PreviewPagination, LayoutCssSizesPageBox)
{
    QString css = PreviewPagination::layoutCss(680, 900, 30, 40, 50);
    EXPECT_TRUE(css.contains("#scriba-content{width:680px"));
    EXPECT_TRUE(css.contains("min-height:900px"));
    EXPECT_TRUE(css.contains("padding:30px 40px 50px"));
    EXPECT_TRUE(css.contains("background:#d9d9d9!important"));
}

TEST(PreviewPagination, LayoutCssStylesSeparatorsAndMarkers)
{
    QString css = PreviewPagination::layoutCss(680, 900, 30, 40, 50);
    EXPECT_TRUE(css.contains(".scriba-pb"));
    EXPECT_TRUE(css.contains(".scriba-split-marker"));
    EXPECT_TRUE(css.contains("pre{position:relative;"));
}

TEST(PreviewPagination, PaginatorScriptEmbedsGeometry)
{
    PrintOptions::Options opts;
    QString js = PreviewPagination::paginatorScript(opts, 842);
    EXPECT_TRUE(js.contains("var contentH=842"));
    EXPECT_TRUE(js.contains("window.scribaPaginate"));
    EXPECT_TRUE(js.contains("window.scribaFitZoom"));
}

TEST(PreviewPagination, PaginatorScriptEmbedsOptions)
{
    PrintOptions::Options opts;
    opts.codeSplit = PrintOptions::CodeSplit::SplitLarge;
    opts.keepTables = false;
    opts.keepHeadings = true;
    opts.keepFigures = false;
    opts.orphanControl = false;
    QString js = PreviewPagination::paginatorScript(opts, 842);
    EXPECT_TRUE(js.contains("split:true"));
    EXPECT_TRUE(js.contains("keepTables:false"));
    EXPECT_TRUE(js.contains("keepHeadings:true"));
    EXPECT_TRUE(js.contains("keepFigures:false"));
    EXPECT_TRUE(js.contains("orphanControl:false"));
}

TEST(PreviewPagination, PaginatorScriptNeverSplitFlag)
{
    PrintOptions::Options opts;
    opts.codeSplit = PrintOptions::CodeSplit::NeverSplit;
    QString js = PreviewPagination::paginatorScript(opts, 842);
    EXPECT_TRUE(js.contains("split:false"));
}

// ---------- WebEngine integration tests ----------

namespace {

constexpr int kContentHpx = 300;

PrintOptions::Options defaultOpts()
{
    PrintOptions::Options opts;
    opts.codeSplit = PrintOptions::CodeSplit::NeverSplit;
    opts.keepTables = true;
    opts.keepHeadings = true;
    opts.keepFigures = true;
    opts.orphanControl = true;
    return opts;
}

QString buildTestHtml(const PrintOptions::Options &opts, const QString &blocks)
{
    QString layout = PreviewPagination::layoutCss(500, kContentHpx, 20, 20, 20);
    return QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>%1</style></head>"
        "<body id=\"preview\"><div id=\"scriba-content\">%2</div>%3"
        "</body></html>")
        .arg(layout)
        .arg(blocks)
        .arg(PreviewPagination::paginatorScript(opts, kContentHpx));
}

QString keepBlock(int h)
{
    return QStringLiteral("<div class=\"scriba-keep\" style=\"height:%1px;margin:0\">keep</div>").arg(h);
}

class PreviewPaginationEngine : public QWebEngineView
{
public:
    PreviewPaginationEngine(const QString &html)
    {
        resize(900, 800);
        show();
        QTest::qWait(50);
        setHtml(html);
        waitLoaded();
    }

    void waitLoaded(int maxWaitMs = 8000)
    {
        bool loaded = false;
        connect(page(), &QWebEnginePage::loadFinished, this,
                [&loaded](bool ok) { loaded = ok; });
        for (int elapsed = 0; elapsed < maxWaitMs && !loaded; elapsed += 50)
            QTest::qWait(50);
        // Let the synchronous fitZoom() call in the paginator land too.
        QTest::qWait(100);
    }

    QString eval(const QString &js)
    {
        QVariant result;
        page()->runJavaScript(js, [&result](const QVariant &r) { result = r; });
        for (int elapsed = 0; elapsed < 4000 && result.isNull(); elapsed += 25)
            QTest::qWait(25);
        return result.toString();
    }

    int paginate()
    {
        return eval("window.scribaPaginate()").toInt();
    }

    int count(const char *selector)
    {
        return eval(QStringLiteral("document.querySelectorAll('%1').length").arg(selector)).toInt();
    }
};

} // namespace

TEST(PreviewPaginationEngine, PaginatesKeptBlocksAcrossPages)
{
    QString blocks = keepBlock(150) + keepBlock(150) + keepBlock(150)
                   + keepBlock(150) + keepBlock(150);
    PreviewPaginationEngine eng(buildTestHtml(defaultOpts(), blocks));

    EXPECT_EQ(eng.paginate(), 2);
    EXPECT_EQ(eng.count(".scriba-pb"), 2);
    EXPECT_EQ(eng.count(".scriba-split-marker"), 0);
}

TEST(PreviewPaginationEngine, PaginateIsIdempotent)
{
    QString blocks = keepBlock(150) + keepBlock(150) + keepBlock(150)
                   + keepBlock(150) + keepBlock(150);
    PreviewPaginationEngine eng(buildTestHtml(defaultOpts(), blocks));

    eng.paginate();
    EXPECT_EQ(eng.count(".scriba-pb"), 2);
    // Re-running must not duplicate separators.
    eng.paginate();
    EXPECT_EQ(eng.count(".scriba-pb"), 2);
}

TEST(PreviewPaginationEngine, KeepsBlockTogetherWhenItFits)
{
    // Two 280px keep blocks on a 300px page: the second cannot fit after the
    // first, so it is pushed to its own page.
    PreviewPaginationEngine eng(buildTestHtml(
        defaultOpts(), keepBlock(280) + keepBlock(280)));

    EXPECT_EQ(eng.paginate(), 1);
    EXPECT_EQ(eng.count(".scriba-pb"), 1);
}

TEST(PreviewPaginationEngine, ExplicitPageBreakForcesSeparator)
{
    QString blocks = keepBlock(100)
        + QStringLiteral("<div class=\"scriba-page-break\" style=\"height:100px;margin:0\">pb</div>")
        + keepBlock(100);
    PreviewPaginationEngine eng(buildTestHtml(defaultOpts(), blocks));

    EXPECT_EQ(eng.paginate(), 1);
    EXPECT_EQ(eng.count(".scriba-pb"), 1);
}

TEST(PreviewPaginationEngine, OversizedPreSplitsWithMarkers)
{
    PrintOptions::Options opts = defaultOpts();
    opts.codeSplit = PrintOptions::CodeSplit::SplitLarge;
    QString blocks = QStringLiteral("<pre style=\"height:700px;margin:0\">code</pre>");
    PreviewPaginationEngine eng(buildTestHtml(opts, blocks));

    EXPECT_EQ(eng.paginate(), 0);
    EXPECT_EQ(eng.count(".scriba-split-marker"), 2);
    EXPECT_EQ(eng.count(".scriba-pb"), 0);
}

TEST(PreviewPaginationEngine, OversizedPreDoesNotSplitWhenDisabled)
{
    PrintOptions::Options opts = defaultOpts();
    opts.codeSplit = PrintOptions::CodeSplit::NeverSplit;
    QString blocks = QStringLiteral("<pre style=\"height:700px;margin:0\">code</pre>");
    PreviewPaginationEngine eng(buildTestHtml(opts, blocks));

    EXPECT_EQ(eng.paginate(), 0);
    EXPECT_EQ(eng.count(".scriba-split-marker"), 0);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
