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
#include <QTemporaryDir>
#include "HtmlToOoxml.h"
#include "DocxExporter.h"
#include "ZipReader.h"
#include "TestConfig.h"

class DocxExportTest : public testing::Test
{
protected:
    OoxmlResult convert(const QString &html)
    {
        return HtmlToOoxml::convert(html);
    }
};

TEST_F(DocxExportTest, TaskListDoesNotConsumeSubsequentContent)
{
    // Task list items contain <input> void elements. Before the fix,
    // processInlineChildren would recurse into <input> waiting for </input>
    // (which never exists), consuming all subsequent content.
    QString html =
        "<ul>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\" checked> Task done"
        "</li>"
        "</ul>"
        "<h2>After List</h2>"
        "<p>This paragraph must survive.</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("After List"))
        << "Heading after task list must be present in OOXML output";
    EXPECT_TRUE(result.bodyXml.contains("must survive"))
        << "Paragraph after task list must be present in OOXML output";
}

TEST_F(DocxExportTest, MultipleTaskListsDoNotConsumeContent)
{
    QString html =
        "<ul>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\" checked> Done"
        "</li>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\"> Pending"
        "</li>"
        "</ul>"
        "<h2>Tables</h2>"
        "<table><tr><td>Cell</td></tr></table>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Tables"))
        << "Heading after multiple task lists must survive";
    EXPECT_TRUE(result.bodyXml.contains("Cell"))
        << "Table after task lists must survive";
}

TEST_F(DocxExportTest, HrVoidElementDoesNotConsumeContent)
{
    QString html =
        "<p>Before</p>"
        "<hr>"
        "<p>After</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Before"))
        << "Content before <hr> must survive";
    EXPECT_TRUE(result.bodyXml.contains("After"))
        << "Content after <hr> must survive";
}

TEST_F(DocxExportTest, InputInsideLiStillExtractsText)
{
    QString html =
        "<ul>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\" checked> Buy groceries"
        "</li>"
        "</ul>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Buy groceries"))
        << "Text content of task list item must be extracted";
}

TEST_F(DocxExportTest, TableAfterListPreserved)
{
    QString html =
        "<ul>"
        "<li>Item 1</li>"
        "<li>Item 2</li>"
        "</ul>"
        "<table>"
        "<thead><tr><th>Col A</th><th>Col B</th></tr></thead>"
        "<tbody><tr><td>1</td><td>2</td></tr></tbody>"
        "</table>"
        "<h2>Code</h2>"
        "<pre><code>int x = 1;</code></pre>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Col A"))
        << "Table headers must survive";
    EXPECT_TRUE(result.bodyXml.contains("Code"))
        << "Content after table must survive";
    EXPECT_TRUE(result.bodyXml.contains("int x = 1"))
        << "Code after table must survive";
}

TEST_F(DocxExportTest, KaTeXSpansProduceText)
{
    // The OMML handler reads the data-tex attribute and emits it as Office Math.
    // The complex inner katex-mathml/katex-html structure is stripped by
    // JavaScript before the HTML reaches the C++ converter.
    QString html =
        "<p>Math: "
        "<span class=\"katex\" data-tex=\"E = mc^2\"></span>"
        "</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Math"))
        << "Text before KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("E = mc^2"))
        << "TeX source must be extracted from data-tex attribute";
}

TEST_F(DocxExportTest, KaTeXAfterTaskListPreserved)
{
    // The critical regression: KaTeX after task list checkboxes was lost
    QString html =
        "<ul>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\" checked> Task"
        "</li>"
        "</ul>"
        "<p>"
        "<span class=\"katex\" data-tex=\"x\"></span>"
        "</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("x"))
        << "KaTeX content after task list must survive";
}

TEST_F(DocxExportTest, EmptyDocumentProducesValidXml)
{
    OoxmlResult result = convert("<p></p>");
    EXPECT_FALSE(result.bodyXml.isEmpty());
}

TEST_F(DocxExportTest, NormalListFollowedByTable)
{
    // Non-task lists should also work correctly
    QString html =
        "<ol>"
        "<li>First</li>"
        "<li>Second</li>"
        "</ol>"
        "<table><tr><td>Data</td></tr></table>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Data"))
        << "Table after ordered list must survive";
}

TEST_F(DocxExportTest, BlockquoteAfterTaskList)
{
    QString html =
        "<ul>"
        "<li class=\"task-list-item\">"
        "<input type=\"checkbox\"> Todo"
        "</li>"
        "</ul>"
        "<blockquote><p>Important note</p></blockquote>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Important note"))
        << "Blockquote after task list must survive";
}

TEST_F(DocxExportTest, CodeBlockPreservesNewlines)
{
    // Code blocks contain newlines that must be preserved as <w:br/> in OOXML
    QString html =
        "<pre><code>line one\n"
        "line two\n"
        "line three</code></pre>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("line one"))
        << "First line must be present";
    EXPECT_TRUE(result.bodyXml.contains("line two"))
        << "Second line must be present";
    EXPECT_TRUE(result.bodyXml.contains("line three"))
        << "Third line must be present";
    // Verify <w:br/> is emitted between lines
    EXPECT_TRUE(result.bodyXml.contains("<w:br/>") || result.bodyXml.contains("<w:br />"))
        << "Line breaks must be emitted as <w:br/>";
}

TEST_F(DocxExportTest, CodeBlockNewlinesNotCollapsed)
{
    // OOXML collapses whitespace in <w:t>, so newlines must be split into <w:br/>
    QString html = "<pre><code>a\nb</code></pre>";

    OoxmlResult result = convert(html);

    // The text "a" and "b" should be in separate runs
    EXPECT_TRUE(result.bodyXml.contains("a"));
    EXPECT_TRUE(result.bodyXml.contains("b"));
    // Must contain a line break
    EXPECT_TRUE(result.bodyXml.contains("w:br"))
        << "Newline in code must become a break element";
}

TEST_F(DocxExportTest, AdmonitionTitleUsesAdmonitionTitleStyle)
{
    // Admonition title paragraphs should use the AdmonitionTitle style
    QString html =
        "<div class=\"admonition note\">"
        "<p class=\"admonition-title\">Note</p>"
        "<p>This is a note.</p>"
        "</div>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("AdmonitionTitlenote"))
        << "Admonition title paragraph must use AdmonitionTitlenote style";
    EXPECT_TRUE(result.bodyXml.contains("Note"))
        << "Admonition title text must be present";
}

TEST_F(DocxExportTest, AdmonitionTitleBodyUsesNormalStyle)
{
    // Body paragraphs inside admonitions should use Normal style
    QString html =
        "<div class=\"admonition warning\">"
        "<p class=\"admonition-title\">Warning</p>"
        "<p>Be careful!</p>"
        "</div>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Warning"))
        << "Admonition title must be present";
    EXPECT_TRUE(result.bodyXml.contains("Be careful"))
        << "Admonition body must be present";
}

TEST_F(DocxExportTest, StylesXmlContainsAdmonitionTitleStyle)
{
    // The styles.xml must contain per-type admonition styles
    QString stylesXml = HtmlToOoxml::buildStylesXml("");

    EXPECT_TRUE(stylesXml.contains("AdmonitionTitlenote"))
        << "styles.xml must contain AdmonitionTitlenote style";
    EXPECT_TRUE(stylesXml.contains("AdmonitionTextnote"))
        << "styles.xml must contain AdmonitionTextnote style";
    EXPECT_TRUE(stylesXml.contains("AdmonitionTextwarning"))
        << "styles.xml must contain AdmonitionTextwarning style";
    EXPECT_TRUE(stylesXml.contains("Admonition note Title"))
        << "styles.xml must contain admonition title display name";
    EXPECT_TRUE(stylesXml.contains("w:between"))
        << "styles.xml must have w:between for admonition grouping";
}

TEST_F(DocxExportTest, StyleNameUsesValAttribute)
{
    // w:name is CT_String and must use w:val attribute, not text content
    QString stylesXml = HtmlToOoxml::buildStylesXml("");
    EXPECT_TRUE(stylesXml.contains("w:name w:val="))
        << "w:name must use w:val attribute per OOXML spec";
    EXPECT_FALSE(stylesXml.contains("<w:name>Normal<"))
        << "w:name must not use text content";
}

TEST_F(DocxExportTest, StyleBasedOnUsesValAttribute)
{
    // w:basedOn is CT_String and must use w:val attribute, not text content
    QString stylesXml = HtmlToOoxml::buildStylesXml("");
    EXPECT_TRUE(stylesXml.contains("w:basedOn w:val="))
        << "w:basedOn must use w:val attribute per OOXML spec";
    EXPECT_FALSE(stylesXml.contains("<w:basedOn>Normal<"))
        << "w:basedOn must not use text content";
}

TEST_F(DocxExportTest, KaTeXDisplayMathProducesOmml)
{
    // Display math inside katex-display div should produce OMML with w:p wrapper
    QString html =
        "<div class=\"katex-display\">"
        "<span class=\"katex\" data-tex=\"\\frac{n!}{k!(n-k)!}\"></span>"
        "</div>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("m:oMath"))
        << "Display math must produce OMML oMath element";
    EXPECT_TRUE(result.bodyXml.contains("\\frac{n!}{k!(n-k)!"))
        << "TeX source must be extracted from data-tex attribute";
}

TEST_F(DocxExportTest, MultipleKaTeXElementsAllPreserved)
{
    // Two inline KaTeX elements with text between them must all survive
    QString html =
        "<p>Before "
        "<span class=\"katex\" data-tex=\"x\"></span>"
        " between "
        "<span class=\"katex\" data-tex=\"y\"></span>"
        " after</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Before"))
        << "Text before first KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("between"))
        << "Text between KaTeX elements must survive";
    EXPECT_TRUE(result.bodyXml.contains("after"))
        << "Text after second KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("x"))
        << "First KaTeX TeX source must be present";
    EXPECT_TRUE(result.bodyXml.contains("y"))
        << "Second KaTeX TeX source must be present";
}

TEST_F(DocxExportTest, ContentAfterKaTeXPreservedInOmmlMode)
{
    // KaTeX followed by paragraph, table, and admonition must all survive
    QString html =
        "<p>Math: "
        "<span class=\"katex\" data-tex=\"a\"></span>"
        "</p>"
        "<p>After math paragraph</p>"
        "<table><tr><td>Cell</td></tr></table>"
        "<div class=\"admonition note\">"
        "<p class=\"admonition-title\">Note</p>"
        "<p>Admonition body</p>"
        "</div>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("After math paragraph"))
        << "Paragraph after KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("Cell"))
        << "Table after KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("Note"))
        << "Admonition title after KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("AdmonitionTitlenote"))
        << "Admonition title must use AdmonitionTitlenote style";
}

TEST_F(DocxExportTest, BlockLevelImgProducesDrawing)
{
    // Block-level <img> (e.g. from KaTeX image conversion) should produce a drawing
    QString html =
        "<div class=\"katex-display\">"
        "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==\">"
        "</div>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("w:drawing"))
        << "Block-level <img> must produce a w:drawing element";
    EXPECT_TRUE(result.bodyXml.contains("wp:inline"))
        << "Block-level image must use inline drawing";
}

TEST_F(DocxExportTest, KaTeXDisplayMathFollowedByContent)
{
    // Display math div followed by a paragraph — content after must survive
    QString html =
        "<div class=\"katex-display\">"
        "<span class=\"katex\" data-tex=\"E\"></span>"
        "</div>"
        "<p>After display math</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("E"))
        << "KaTeX TeX source must be present";
    EXPECT_TRUE(result.bodyXml.contains("After display math"))
        << "Content after display math must survive";
}

// ── new feature tests ────────────────────────────────────────────────────────

TEST_F(DocxExportTest, InlineStyleUnderlineProducesU)
{
    QString html = "<p style=\"text-decoration: underline\">underlined</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:u"))
        << "text-decoration:underline must produce w:u element";
    EXPECT_TRUE(result.bodyXml.contains("underlined"))
        << "Content must survive";
}

TEST_F(DocxExportTest, InlineStyleLineThroughProducesStrike)
{
    QString html = "<p style=\"text-decoration: line-through\">struck</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:strike"))
        << "text-decoration:line-through must produce w:strike element";
}

TEST_F(DocxExportTest, DelTagProducesStrike)
{
    QString html = "<p><del>deleted</del></p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:strike"))
        << "<del> tag must produce w:strike";
    EXPECT_TRUE(result.bodyXml.contains("deleted"))
        << "Content inside <del> must survive";
}

TEST_F(DocxExportTest, StrikeTagProducesStrike)
{
    QString html = "<p><s>struck</s></p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:strike"))
        << "<s> tag must produce w:strike";
}

TEST_F(DocxExportTest, UTagProducesUnderline)
{
    QString html = "<p><u>underlined</u></p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:u"))
        << "<u> tag must produce w:u element";
}

TEST_F(DocxExportTest, InsTagProducesUnderline)
{
    QString html = "<p><ins>inserted</ins></p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:u"))
        << "<ins> tag must produce w:u element";
}

TEST_F(DocxExportTest, FontFamilyFromInlineStyle)
{
    QString html = "<p style=\"font-family: 'Roboto', sans-serif\">Roboto text</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("Roboto text"))
        << "Content with font-family must survive";
    EXPECT_TRUE(result.bodyXml.contains("w:rFonts"))
        << "font-family must produce w:rFonts element";
}

TEST_F(DocxExportTest, TextShadowProducesShadow)
{
    QString html = "<p style=\"text-shadow: 2px 2px 4px #000\">shadowed</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:shadow"))
        << "text-shadow must produce w:shadow element";
    EXPECT_TRUE(result.bodyXml.contains("shadowed"))
        << "Content must survive";
}

TEST_F(DocxExportTest, UnderlineAndLineThroughTogether)
{
    // Both must appear in the output when both CSS properties are set
    QString html = "<p style=\"text-decoration: underline line-through\">both</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:u"))
        << "underline must be present";
    EXPECT_TRUE(result.bodyXml.contains("w:strike"))
        << "line-through must be present";
}

TEST_F(DocxExportTest, WPPrSpecOrdering)
{
    // Verify that w:rPr children appear in ECMA-376 spec order
    QString html = "<p><strong><em style=\"text-decoration: underline\">text</em></strong></p>";
    OoxmlResult result = convert(html);
    // Check that w:b comes before w:i before w:u in the serialized XML
    QString body = result.bodyXml;
    int posB = body.indexOf("w:b");
    int posI = body.indexOf("w:i");
    int posU = body.indexOf("w:u");
    // w:b (slot 3), w:i (slot 5), w:u (slot 27) — must be in order
    EXPECT_LT(posB, posI) << "w:b must come before w:i in spec order";
    EXPECT_LT(posI, posU) << "w:i must come before w:u in spec order";
}

TEST_F(DocxExportTest, MarginLeftProducesIndent)
{
    QString html = "<p style=\"margin-left: 40px\">indented</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:ind"))
        << "margin-left must produce w:ind element";
    EXPECT_TRUE(result.bodyXml.contains("indented"))
        << "Content must survive";
}

TEST_F(DocxExportTest, MarginBottomProducesSpacing)
{
    QString html = "<p style=\"margin-bottom: 12pt\">spaced</p>";
    OoxmlResult result = convert(html);
    EXPECT_TRUE(result.bodyXml.contains("w:spacing"))
        << "margin-bottom must produce w:spacing element";
}

TEST_F(DocxExportTest, ZipEntriesAreDeflateCompressedAndRoundTrip)
{
    // The exported .docx must use DEFLATE (method 8) for compressible entries
    // and remain readable back through ZipReader (uncompressing correctly).
    QString html = QStringLiteral(
        "<h1>Hello</h1>"
        "<p>This is a test paragraph whose repeated words make it highly "
        "compressible compressible compressible compressible.</p>"
        "<table><tr><td>A</td><td>B</td></tr></table>");

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString outPath = dir.path() + QLatin1String("/roundtrip.docx");

    ASSERT_TRUE(DocxExporter::exportToDocx(html, outPath, QString(), DocxExportOptions()))
        << "exportToDocx must succeed";

    ZipReader zip(outPath);
    QString error;
    ASSERT_TRUE(zip.open(&error)) << "Exported zip must open: " << qPrintable(error);

    const QStringList names = zip.entryNames();
    EXPECT_TRUE(names.contains(QStringLiteral("word/document.xml")))
        << "Package must contain the main document part";

    // The document part contains the exported content (proves DEFLATE decompression)
    QByteArray doc = zip.readEntry(QStringLiteral("word/document.xml"));
    EXPECT_FALSE(doc.isEmpty());
    EXPECT_TRUE(doc.contains("<w:body>"))
        << "document.xml must decompress to valid OOXML body XML";
    EXPECT_TRUE(doc.contains("compressible"))
        << "document.xml must contain the exported paragraph text";

    // Compressible XML parts must actually be smaller than their raw content,
    // i.e. DEFLATE ran rather than falling through to STORE.
    QByteArray styles = zip.readEntry(QStringLiteral("word/styles.xml"));
    EXPECT_FALSE(styles.isEmpty());
    EXPECT_GT(styles.size(), 0);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
