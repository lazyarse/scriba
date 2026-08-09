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
#include <QHash>
#include <QString>
#include <QStringList>

#include "OoxmlToHtml.h"

namespace {

// Build a minimal, complete Word (OOXML) package as the parts map that
// OoxmlToHtml::convert consumes. The caller only needs to hand the body XML;
// the boilerplate parts (rels, styles, numbering, [content types is not read])
// are filled in automatically.
QHash<QString, QByteArray> makePackage(const QByteArray &bodyXml,
                                       const char *stylesXml = "",
                                       const char *numberingXml = "",
                                       const char *footnotesXml = "")
{
    QHash<QString, QByteArray> parts;
    parts.insert(QStringLiteral("word/document.xml"), bodyXml);

    // Relationships: default set covers images, hyperlinks, footnotes,
    // headers and footers as referenced by each test's body XML.
    parts.insert(QStringLiteral("word/_rels/document.xml.rels"),
                 "<?xml version=\"1.0\"?>"
                 "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                 "<Relationship Id=\"rIdImg1\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
                 "Target=\"media/image1.png\"/>"
                 "<Relationship Id=\"rIdLink1\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "
                 "Target=\"https://example.com/page\" TargetMode=\"External\"/>"
                 "<Relationship Id=\"rIdHdr1\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" "
                 "Target=\"header1.xml\"/>"
                 "<Relationship Id=\"rIdFtr1\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer\" "
                 "Target=\"footer1.xml\"/>"
                 "</Relationships>");

    if (*stylesXml)
        parts.insert(QStringLiteral("word/styles.xml"),
                     QByteArray("<?xml version=\"1.0\"?><w:styles "
                                "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">")
                         + stylesXml + "</w:styles>");
    if (*numberingXml)
        parts.insert(QStringLiteral("word/numbering.xml"), numberingXml);
    if (*footnotesXml)
        parts.insert(QStringLiteral("word/footnotes.xml"), footnotesXml);

    parts.insert(QStringLiteral("word/media/image1.png"),
                 QByteArray("\x89PNG\r\n\x1a\nfake data for round-trip", 30));
    parts.insert(QStringLiteral("word/header1.xml"),
                 "<w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                 "<w:p><w:r><w:t>My Header</w:t></w:r></w:p></w:hdr>");
    parts.insert(QStringLiteral("word/footer1.xml"),
                 "<w:ftr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                 "<w:p><w:r><w:t>Page 1 of 1</w:t></w:r></w:p></w:ftr>");
    return parts;
}

const char *kDefaultW = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
                        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"";

const char *kNumberingXml =
    "<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
    "<w:abstractNum w:abstractNumId=\"100\">"
    "<w:lvl><w:ilvl w:val=\"0\"/><w:numFmt w:val=\"bullet\"/></w:lvl>"
    "</w:abstractNum>"
    "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"100\"/></w:num>"
    "</w:numbering>";

} // namespace

TEST(OoxmlToHtmlTest, ParagraphRunsAndFormatting)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:r><w:t>Plain text</w:t></w:r></w:p>"
        "<w:p><w:r><w:rPr><w:b/><w:i/><w:u/></w:rPr><w:t>bold italic underline</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<p>Plain text</p>"));
    EXPECT_TRUE(res.html.contains("<strong><em><u>bold italic underline</u></em></strong>"));
}

TEST(OoxmlToHtmlTest, HeadingsFromStyles)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>"
        "<w:r><w:t>Chapter One</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:pStyle w:val=\"Heading2\"/></w:pPr>"
        "<w:r><w:t>Section A</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:outlineLvl w:val=\"2\"/></w:pPr>"
        "<w:r><w:t>Deep</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>",
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\">"
        "<w:name w:val=\"heading 1\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\">"
        "<w:name w:val=\"heading 2\"/></w:style>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<h1>Chapter One</h1>"));
    EXPECT_TRUE(res.html.contains("<h2>Section A</h2>"));
    EXPECT_TRUE(res.html.contains("<h3>Deep</h3>"));
}

TEST(OoxmlToHtmlTest, BulletAndNumberedLists)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>First</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Second</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"1\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Nested</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"2\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Ordered One</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>After list</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>",
        "",
        "<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:abstractNum w:abstractNumId=\"100\">"
        "<w:lvl><w:ilvl w:val=\"0\"/><w:numFmt w:val=\"bullet\"/></w:lvl>"
        "<w:lvl><w:ilvl w:val=\"1\"/><w:numFmt w:val=\"bullet\"/></w:lvl>"
        "</w:abstractNum>"
        "<w:abstractNum w:abstractNumId=\"101\">"
        "<w:lvl><w:ilvl w:val=\"0\"/><w:numFmt w:val=\"decimal\"/></w:lvl>"
        "</w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"100\"/></w:num>"
        "<w:num w:numId=\"2\"><w:abstractNumId w:val=\"101\"/></w:num>"
        "</w:numbering>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<ul>\n<li>First</li>"));
    EXPECT_TRUE(res.html.contains("<li>Second</li>"));
    EXPECT_TRUE(res.html.contains("<li>Nested</li>"));
    EXPECT_TRUE(res.html.contains("<ol>\n<li>Ordered One</li>"));
    EXPECT_TRUE(res.html.contains("<p>After list</p>")) << qPrintable(res.html);
}

TEST(OoxmlToHtmlTest, TableWithHeaderRowAndColspan)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:tbl>"
        "<w:tr><w:trPr><w:tblHeader/></w:trPr>"
        "<w:tc><w:tcPr><w:gridSpan w:val=\"2\"/></w:tcPr>"
        "<w:p><w:r><w:t>Merged Header</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>Last</w:t></w:r></w:p></w:tc>"
        "</w:tr>"
        "<w:tr><w:tc><w:p><w:r><w:t>a</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>b</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>c</w:t></w:r></w:p></w:tc></w:tr>"
        "</w:tbl>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<thead>"));
    EXPECT_TRUE(res.html.contains("<th colspan=\"2\"><p>Merged Header</p>"));
    EXPECT_TRUE(res.html.contains("<th><p>Last</p>"));
    EXPECT_TRUE(res.html.contains("<tbody>"));
    EXPECT_TRUE(res.html.contains("<td><p>a</p>"));
    EXPECT_TRUE(res.html.contains("<td><p>c</p>"));
    EXPECT_TRUE(res.html.contains("</table>"));
}

TEST(OoxmlToHtmlTest, HyperlinkAndExternalImage)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:hyperlink r:id=\"rIdLink1\"><w:r><w:t>Example</w:t></w:r></w:hyperlink></w:p>"
        "<w:p><w:r><w:drawing>"
        "<wp:inline xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\">"
        "<wp:extent cx=\"914400\" cy=\"914400\"/>"
        "<a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
        "<a:graphicData>"
        "<pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        "<pic:blipFill><a:blip r:embed=\"rIdImg1\"/></pic:blipFill>"
        "</pic:pic>"
        "</a:graphicData></a:graphic>"
        "</wp:inline></w:drawing></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<a href=\"https://example.com/page\">Example</a>"));
    EXPECT_TRUE(res.html.contains("<img src=\"docximg://rIdImg1\""));
    ASSERT_EQ(res.images.size(), 1);
    EXPECT_EQ(res.images.at(0).rId, QStringLiteral("rIdImg1"));
    EXPECT_EQ(res.images.at(0).fileName, QStringLiteral("image1.png"));
    EXPECT_EQ(res.images.at(0).contentType, QStringLiteral("image/png"));
    EXPECT_FALSE(res.images.at(0).data.isEmpty());
}

TEST(OoxmlToHtmlTest, FootnoteReferenceAndDefinition)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:r><w:t>Some text</w:t></w:r>"
        "<w:r><w:footnoteReference w:id=\"2\"/></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>",
        "",
        "",
        "<w:footnotes xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:footnote w:type=\"separator\" w:id=\"-1\"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>"
        "<w:footnote w:id=\"2\"><w:p><w:r><w:t>Note two.</w:t></w:r></w:p></w:footnote>"
        "</w:footnotes>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("[^f2]"));
    EXPECT_TRUE(res.html.contains("[^f2]: Note two."));
}

TEST(OoxmlToHtmlTest, HeaderAndFooterBecomeFootnotes)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:r><w:t>Body line</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.startsWith("<p>[^hdr1]</p>"));
    EXPECT_TRUE(res.html.contains("<p>[^ftr1]</p>"));
    EXPECT_TRUE(res.html.contains("[^hdr1]: My Header"));
    EXPECT_TRUE(res.html.contains("[^ftr1]: Page 1 of 1"));
}

TEST(OoxmlToHtmlTest, OfficeMathBecomesLatex)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:r><m:oMath "
        "xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">"
        "<m:r><m:t>x+1</m:t></m:r>"
        "</m:oMath></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains('$')) << qPrintable(res.html);
    EXPECT_TRUE(res.html.contains("x")) << qPrintable(res.html);
}

TEST(OoxmlToHtmlTest, EmptyOrMissingDocumentFails)
{
    OoxmlToHtmlResult empty = OoxmlToHtml::convert({});
    EXPECT_FALSE(empty.ok);
    ASSERT_FALSE(empty.errors.isEmpty());

    QHash<QString, QByteArray> noBody;
    noBody.insert(QStringLiteral("word/document.xml"), "<w:document " + QByteArray(kDefaultW) + "></w:document>");
    OoxmlToHtmlResult nobody = OoxmlToHtml::convert(noBody);
    EXPECT_TRUE(nobody.ok);
    EXPECT_TRUE(nobody.html.isEmpty());
}

TEST(OoxmlToHtmlTest, VmMergeRestartWinsContinuationEmptied)
{
    // First column is vertically merged across two rows: row 1 restarts
    // (keeps "Top"), row 2 continues (text "concealed" is dropped).
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:tbl>"
        "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"restart\"/></w:tcPr>"
        "<w:p><w:r><w:t>Top</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>b1</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:vMerge/></w:tcPr>"
        "<w:p><w:r><w:t>concealed</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>b2</w:t></w:r></w:p></w:tc></w:tr>"
        "</w:tbl>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<td><p>Top</p>")) << qPrintable(res.html);
    EXPECT_TRUE(res.html.contains("<td>&nbsp;</td>")) << qPrintable(res.html);
    EXPECT_FALSE(res.html.contains("concealed")) << qPrintable(res.html);
}

TEST(OoxmlToHtmlTest, VmMergeExplicitContinueAlsoEmptied)
{
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:tbl>"
        "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"restart\"/></w:tcPr><w:p><w:r><w:t>Keep</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"continue\"/></w:tcPr><w:p><w:r><w:t>gone</w:t></w:r></w:p></w:tc></w:tr>"
        "</w:tbl>"
        "<w:sectPr/></w:body></w:document>");
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.html.contains("<td><p>Keep</p>")) << qPrintable(res.html);
    EXPECT_TRUE(res.html.contains("<td>&nbsp;</td>")) << qPrintable(res.html);
    EXPECT_FALSE(res.html.contains("gone")) << qPrintable(res.html);
}

TEST(OoxmlToHtmlTest, LooseListFromInlineSpacing)
{
    const QByteArray bodyTight =
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Tight item</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>";
    OoxmlToHtmlResult tight = OoxmlToHtml::convert(makePackage(bodyTight, "", kNumberingXml));
    ASSERT_TRUE(tight.ok) << qPrintable(tight.errors.join("; "));
    EXPECT_TRUE(tight.html.contains("<ul>\n<li>Tight item</li>")) << qPrintable(tight.html);

    const QByteArray bodyLoose =
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr>"
        "<w:spacing w:before=\"120\" w:after=\"120\"/></w:pPr>"
        "<w:r><w:t>Loose item</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>";
    OoxmlToHtmlResult loose = OoxmlToHtml::convert(makePackage(bodyLoose, "", kNumberingXml));
    ASSERT_TRUE(loose.ok) << qPrintable(loose.errors.join("; "));
    EXPECT_TRUE(loose.html.contains("<ul>\n<li><p>Loose item</p></li>")) << qPrintable(loose.html);
}

TEST(OoxmlToHtmlTest, EmfImageSkippedWithWarning)
{
    // Garbage EMF bytes cannot be rasterized -> skipped, no image registered.
    QHash<QString, QByteArray> parts = makePackage(
        "<w:document " + QByteArray(kDefaultW) + "><w:body>"
        "<w:p><w:r><w:pict><v:imagedata xmlns:v=\"urn:schemas-microsoft-com:vml\" r:id=\"rIdEmf1\"/></w:pict></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>");
    parts.insert(QStringLiteral("word/_rels/document.xml.rels"),
                 "<?xml version=\"1.0\"?>"
                 "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                 "<Relationship Id=\"rIdEmf1\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
                 "Target=\"media/image1.emf\"/></Relationships>");
    parts.insert(QStringLiteral("word/media/image1.emf"),
                 QByteArray("\x01\x00\x00\x00garbage emf bytes for decode failure", 40));
    OoxmlToHtmlResult res = OoxmlToHtml::convert(parts);
    ASSERT_TRUE(res.ok) << qPrintable(res.errors.join("; "));
    EXPECT_TRUE(res.images.isEmpty());
    EXPECT_FALSE(res.errors.isEmpty());
    EXPECT_TRUE(res.errors.join(';').contains("EMF/WMF")) << qPrintable(res.errors.join("; "));
}