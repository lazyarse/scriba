#include <gtest/gtest.h>
#include <QApplication>
#include <QByteArray>
#include <QXmlStreamWriter>
#include "HtmlToOoxml.h"
#include "MathmlToOmml.h"

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

    EXPECT_TRUE(result.bodyXml.contains("AdmonitionTitle"))
        << "Admonition title paragraph must use AdmonitionTitle style";
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
    // The styles.xml must contain the AdmonitionTitle style definition
    QString stylesXml = HtmlToOoxml::buildStylesXml("");

    EXPECT_TRUE(stylesXml.contains("AdmonitionTitle"))
        << "styles.xml must contain AdmonitionTitle style";
    EXPECT_TRUE(stylesXml.contains("Admonition Title"))
        << "styles.xml must contain Admonition Title display name";
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
    EXPECT_TRUE(result.bodyXml.contains("AdmonitionTitle"))
        << "Admonition title must use AdmonitionTitle style";
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

// ── Helpers for direct MathmlToOmml tests ──────────────────────────────────────

static QString ommlFromMathml(const QString &mathmlXml)
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.setAutoFormatting(false);
    w.writeNamespace(QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
                     QStringLiteral("w"));
    w.writeNamespace(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/math"),
                     QStringLiteral("m"));
    w.writeStartElement("x");
    if (!MathmlToOmml::convert(mathmlXml, w))
        return {};
    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf);
}

static QString htmlAttrEncode(const QString &s)
{
    QString r = s;
    r.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    r.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    r.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    r.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
    return r;
}

static QString katexSpanWithMathml(const QString &tex, const QString &mathml)
{
    return QStringLiteral("<span class=\"katex\" data-tex=\"%1\" data-mathml=\"%2\"></span>")
        .arg(tex, htmlAttrEncode(mathml));
}

// ── Direct MathmlToOmml unit tests ────────────────────────────────────────────

TEST(DocxExportMathmlTest, SimpleSuperscript)
{
    QString mml =
        "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">"
        "<semantics>"
        "<mrow><msup><mi>x</mi><mn>2</mn></msup></mrow>"
        "<annotation encoding=\"application/x-tex\">x^2</annotation>"
        "</semantics>"
        "</math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:oMath")) << "Must produce m:oMath element";
    EXPECT_TRUE(omml.contains("<m:sSup>")) << "Superscript must produce m:sSup";
    EXPECT_TRUE(omml.contains("<m:e>")) << "Must have m:e for base";
    EXPECT_TRUE(omml.contains("<m:sup>")) << "Must have m:sup for exponent";
    EXPECT_TRUE(omml.contains("<m:t") && omml.contains(">x<")) << "Base text 'x' must be present";
    EXPECT_TRUE(omml.contains("<m:t") && omml.contains(">2<")) << "Exponent text '2' must be present";
    EXPECT_FALSE(omml.contains("x^2")) << "Raw TeX must not appear in OMML output";
}

TEST(DocxExportMathmlTest, Fraction)
{
    QString mml =
        "<math><semantics>"
        "<mrow><mfrac><mi>a</mi><mi>b</mi></mfrac></mrow>"
        "<annotation encoding=\"application/x-tex\">\\frac{a}{b}</annotation>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:f>")) << "Fraction must produce m:f";
    EXPECT_TRUE(omml.contains("<m:num>")) << "Must have m:num for numerator";
    EXPECT_TRUE(omml.contains("<m:den>")) << "Must have m:den for denominator";
    EXPECT_TRUE(omml.contains(">a<")) << "Numerator text 'a' must be present";
    EXPECT_TRUE(omml.contains(">b<")) << "Denominator text 'b' must be present";
}

TEST(DocxExportMathmlTest, SquareRoot)
{
    QString mml =
        "<math><semantics>"
        "<mrow><msqrt><mi>x</mi></msqrt></mrow>"
        "<annotation encoding=\"application/x-tex\">\\sqrt{x}</annotation>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:rad>")) << "Square root must produce m:rad";
    EXPECT_TRUE(omml.contains("<m:deg/>") || omml.contains("<m:deg />")
                || omml.contains("<m:deg></m:deg>"))
        << "m:deg must be empty for sqrt";
    EXPECT_TRUE(omml.contains(">x<")) << "Radicand text 'x' must be present";
}

TEST(DocxExportMathmlTest, NthRootReordersChildren)
{
    // MathML mroot children: [radicand, degree]
    // OMML order: m:deg (degree), m:e (radicand)
    QString mml =
        "<math><semantics>"
        "<mrow><mroot><mi>x</mi><mn>3</mn></mroot></mrow>"
        "<annotation encoding=\"application/x-tex\">\\sqrt[3]{x}</annotation>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    // Find m:deg content and m:e content to verify order
    int degPos = omml.indexOf("<m:deg>");
    int ePos = omml.indexOf("<m:e>");
    EXPECT_GE(degPos, 0) << "m:deg must be present";
    EXPECT_GE(ePos, 0) << "m:e must be present";
    // deg must appear before e in the output (OMML order: degree first, then base)
    EXPECT_LT(degPos, ePos) << "m:deg must precede m:e in OMML output";
    EXPECT_TRUE(omml.contains(">3<")) << "Degree value '3' must be present";
    EXPECT_TRUE(omml.contains(">x<")) << "Radicand 'x' must be present";
}

TEST(DocxExportMathmlTest, Subscript)
{
    QString mml =
        "<math><semantics>"
        "<mrow><msub><mi>a</mi><mn>1</mn></msub></mrow>"
        "<annotation encoding=\"application/x-tex\">a_1</annotation>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:sSub>")) << "Subscript must produce m:sSub";
    EXPECT_TRUE(omml.contains("<m:sub>")) << "Must have m:sub element";
    EXPECT_TRUE(omml.contains(">a<")) << "Base 'a' must be present";
    EXPECT_TRUE(omml.contains(">1<")) << "Subscript '1' must be present";
}

TEST(DocxExportMathmlTest, SubSup)
{
    QString mml =
        "<math><semantics>"
        "<mrow><msubsup><mi>A</mi><mn>1</mn><mn>2</mn></msubsup></mrow>"
        "<annotation encoding=\"application/x-tex\">A_1^2</annotation>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:sSubSup>")) << "SubSup must produce m:sSubSup";
    EXPECT_TRUE(omml.contains("<m:sub>")) << "Must have m:sub element";
    EXPECT_TRUE(omml.contains("<m:sup>")) << "Must have m:sup element";
    EXPECT_TRUE(omml.contains(">A<")) << "Base 'A' must be present";
    EXPECT_TRUE(omml.contains(">1<")) << "Subscript '1' must be present";
    EXPECT_TRUE(omml.contains(">2<")) << "Superscript '2' must be present";
}

TEST(DocxExportMathmlTest, ItalicStyle)
{
    // <mi> should be rendered with italic style in OMML
    QString mml =
        "<math><semantics>"
        "<mrow><mi>x</mi></mrow>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:sty>i</m:sty>")) << "mi must use italic style";
}

TEST(DocxExportMathmlTest, OverAndUnderLimits)
{
    QString mml =
        "<math><semantics>"
        "<mrow><mover><mi>x</mi><mo>&#xAF;</mo></mover></mrow>"
        "</semantics></math>";

    QString omml = ommlFromMathml(mml);

    EXPECT_TRUE(omml.contains("<m:limUpp>")) << "mover must produce m:limUpp";
    EXPECT_TRUE(omml.contains(">x<")) << "Base 'x' must be present";
}

TEST(DocxExportMathmlTest, DisplayMathPreservesSurroundingText)
{
    // Block-level display math with data-mathml: content before/after must survive
    QString mml =
        "<math><semantics>"
        "<mrow><mi>E</mi><mo>=</mo><mi>m</mi><msup><mi>c</mi><mn>2</mn></msup></mrow>"
        "<annotation encoding=\"application/x-tex\">E=mc^2</annotation>"
        "</semantics></math>";

    QString html =
        "<div class=\"katex-display\">"
        + katexSpanWithMathml(QStringLiteral("E=mc^2"), mml)
        + "</div>";

    OoxmlResult result = HtmlToOoxml::convert(html);

    EXPECT_TRUE(result.bodyXml.contains("<m:oMath")) << "Must produce OMML element";
    EXPECT_TRUE(result.bodyXml.contains("<m:sSup>")) << "Superscript structure must be present";
    EXPECT_FALSE(result.bodyXml.contains("E=mc^2")) << "Raw TeX must NOT appear in OMML mode";
}

// ── Integration tests: data-mathml via HtmlToOoxml ─────────────────────────────

TEST_F(DocxExportTest, InlineMathWithMathmlProducesOmmlStructure)
{
    // Inline KaTeX with data-mathml — should produce proper OMML structure,
    // not raw LaTeX text
    QString mml =
        "<math><semantics>"
        "<mrow><msup><mi>x</mi><mn>2</mn></msup></mrow>"
        "<annotation encoding=\"application/x-tex\">x^2</annotation>"
        "</semantics></math>";

    QString html =
        "<p>Before "
        + katexSpanWithMathml(QStringLiteral("x^2"), mml)
        + " after</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("Before")) << "Text before KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("after")) << "Text after KaTeX must survive";
    EXPECT_TRUE(result.bodyXml.contains("<m:sSup>")) << "Superscript OMML structure must be present";
    EXPECT_TRUE(result.bodyXml.contains("<m:t") && (result.bodyXml.contains(">x<") || result.bodyXml.contains(">x</")))
        << "Base text 'x' must be present in OMML";
    EXPECT_TRUE(result.bodyXml.contains("<m:t") && (result.bodyXml.contains(">2<") || result.bodyXml.contains(">2</")))
        << "Exponent text '2' must be present in OMML";
    EXPECT_FALSE(result.bodyXml.contains("x^2")) << "Raw TeX must not appear when data-mathml is present";
}

TEST_F(DocxExportTest, DisplayMathWithMathml)
{
    // Display math with data-mathml should produce OMML inside w:p wrapper
    QString mml =
        "<math><semantics>"
        "<mrow><mfrac><mrow><mi>n</mi><mo>!</mo></mrow>"
        "<mrow><mi>k</mi><mo>!</mo><mrow><mo>(</mo><mi>n</mi><mo>-</mo><mi>k</mi><mo>)</mo></mrow><mo>!</mo></mrow></mfrac></mrow>"
        "<annotation encoding=\"application/x-tex\">\\frac{n!}{k!(n-k)!}</annotation>"
        "</semantics></math>";

    QString html =
        "<div class=\"katex-display\">"
        + katexSpanWithMathml(QStringLiteral("\\frac{n!}{k!(n-k)!"), mml)
        + "</div>"
        "<p>After math</p>";

    OoxmlResult result = convert(html);

    EXPECT_TRUE(result.bodyXml.contains("<m:f>")) << "Fraction must produce m:f element";
    EXPECT_TRUE(result.bodyXml.contains("<m:num>")) << "Fraction must have numerator";
    EXPECT_TRUE(result.bodyXml.contains("<m:den>")) << "Fraction must have denominator";
    EXPECT_TRUE(result.bodyXml.contains("After math")) << "Content after display math must survive";
    EXPECT_FALSE(result.bodyXml.contains("\\\\frac")) << "Raw TeX must not appear in OMML mode";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("scribaTest");
    app.setApplicationName("scribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
