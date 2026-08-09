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
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <cstring>

#include "DocxExporter.h"
#include "DocxImporter.h"
#include "TestConfig.h"
#include "miniz.h"

// A tiny 1x1 PNG (valid header, minimal data).
static QByteArray tinyPng()
{
    return QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==");
}

// Build a .docx whose body contains a heading, paragraph, list, table and an
// image (referenced by a data: URI so HtmlToOoxml embeds it as a media part).
static QString buildDocx(const QTemporaryDir &dir)
{
    const QString html = QStringLiteral(
        "<h1>Imported Doc</h1>"
        "<p>Paragraph with <strong>bold</strong> and <em>italic</em>.</p>"
        "<ul><li>item one</li><li>item two</li></ul>"
        "<table><tr><th>H</th></tr><tr><td>C</td></tr></table>"
        "<p><img src=\"data:image/png;base64,%1\" alt=\"pixel\"></p>")
        .arg(QString::fromLatin1(tinyPng().toBase64()));
    const QString out = dir.path() + QStringLiteral("/test.docx");
    if (!DocxExporter::exportToDocx(html, out, QString(), DocxExportOptions()))
        return {};
    return out;
}

// Write a .docx on disk from a raw parts map (entry name -> bytes) using
// miniz, for tests that need hand-crafted OOXML (math, footnotes, headers,
// footers, vMerge) that DocxExporter cannot produce.
static QString buildDocxFromParts(const QTemporaryDir &dir,
                                  const QHash<QString, QByteArray> &parts)
{
    const QString out = dir.path() + QStringLiteral("/parts.docx");

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        return {};
    for (auto it = parts.cbegin(); it != parts.cend(); ++it) {
        const QByteArray name = it.key().toUtf8();
        const QByteArray data = it.value();
        if (!mz_zip_writer_add_mem(&zip, name.constData(), data.constData(),
                                   data.size(), MZ_BEST_COMPRESSION)) {
            mz_zip_writer_end(&zip);
            return {};
        }
    }
    void *buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) {
        mz_zip_writer_end(&zip);
        return {};
    }
    mz_zip_writer_end(&zip);

    QFile file(out);
    if (!file.open(QIODevice::WriteOnly)) {
        mz_free(buf);
        return {};
    }
    file.write(static_cast<const char *>(buf), qint64(size));
    file.close();
    mz_free(buf);
    return out;
}

// Minimal package boilerplate for buildDocxFromParts bodies that need no
// special parts: document.xml + a bare rels file.
static QHash<QString, QByteArray> packageWithBody(const QByteArray &bodyXml)
{
    QHash<QString, QByteArray> parts;
    parts.insert(QStringLiteral("word/document.xml"), bodyXml);
    parts.insert(QStringLiteral("word/_rels/document.xml.rels"),
                 "<?xml version=\"1.0\"?>"
                 "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                 "</Relationships>");
    return parts;
}

const char *kDocxW = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
                     "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"";
const char *kMathML = "xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\"";
const char *kRelNs = "xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"";

// Strips all whitespace so assertions are insensitive to turndown wrapping.
static QString norm(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar &c : s) {
        if (!c.isSpace())
            out.append(c);
    }
    return out;
}

class DocxImportTest : public testing::Test
{
protected:
    QTemporaryDir m_dir;

    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
    }
};

TEST_F(DocxImportTest, ImportsContentIntoMarkdown)
{
    const QString docx = buildDocx(m_dir);
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();
    EXPECT_FALSE(result.markdown.isEmpty());

    const QString md = norm(result.markdown);
    EXPECT_TRUE(md.contains("#ImportedDoc")) << md.toStdString();
    EXPECT_TRUE(md.contains("Paragraphwith**bold**and*italic*")) << md.toStdString();
    EXPECT_TRUE(md.contains("-itemone")) << md.toStdString();
    EXPECT_TRUE(md.contains("-itemtwo")) << md.toStdString();
    // Tables import as raw HTML unless the source marks a header row with
    // <w:tblHeader/> (our exporter doesn't), so assert the content survives.
    EXPECT_TRUE(md.contains("H")) << md.toStdString();
    EXPECT_TRUE(md.contains("C")) << md.toStdString();
}

TEST_F(DocxImportTest, CurrentDirWritesImagesNextToDocument)
{
    const QString docx = buildDocx(m_dir);
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    // Exactly one media file written into <doc>/media/, referenced relatively.
    EXPECT_EQ(result.writtenImages.size(), 1);
    const QString mediaDir = QDir(m_dir.path()).filePath(QStringLiteral("media"));
    EXPECT_TRUE(QDir(mediaDir).exists());
    EXPECT_TRUE(QFile::exists(result.writtenImages.first()));

    const QString md = norm(result.markdown);
    // Alt text isn't preserved through the docx round-trip; assert the ref.
    EXPECT_TRUE(md.contains("](media/image1.png)")) << md.toStdString();
}

TEST_F(DocxImportTest, TempDirWritesImagesToSystemTemp)
{
    const QString docx = buildDocx(m_dir);
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::TempDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    EXPECT_EQ(result.writtenImages.size(), 1);
    // Written into the system temp area (not the document dir).
    EXPECT_FALSE(QDir(m_dir.path()).exists(QStringLiteral("media")));
    EXPECT_FALSE(result.writtenImages.first().startsWith(m_dir.path()));
    EXPECT_TRUE(QFile::exists(result.writtenImages.first()));

    const QString md = norm(result.markdown);
    EXPECT_TRUE(md.contains("](file:")) << md.toStdString();
    EXPECT_FALSE(md.contains("media/image1.png")) << md.toStdString();
}

TEST_F(DocxImportTest, UnsavedDocumentWithCurrentDirEscalatesAndSkipsImages)
{
    const QString docx = buildDocx(m_dir);
    ASSERT_FALSE(docx.isEmpty());

    // documentDir empty -> CurrentDir cannot express a relative ref; importer
    // escalates to Ask (cancelled in tests -> images skipped, not fatal).
    QTimer::singleShot(0, []() {
        if (auto *dlg = qobject_cast<QDialog *>(qApp->activeModalWidget()))
            dlg->reject();
    });
    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = QString();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    EXPECT_TRUE(result.writtenImages.isEmpty());
    EXPECT_FALSE(result.markdown.contains("docximg://")) << "no leaked placeholders";
    // Content itself still imports.
    EXPECT_TRUE(norm(result.markdown).contains("#ImportedDoc"));
}

TEST_F(DocxImportTest, MathFractionImportsAsLatex)
{
    // Hand-built OMML fraction (m:f) -> MathML -> LaTeX -> turndown.
    const QByteArray body =
        "<w:document " + QByteArray(kDocxW) + " " + QByteArray(kMathML) + "><w:body>"
        "<w:p><w:r><m:oMath>"
        "<m:f><m:num><m:r><m:t>a</m:t></m:r></m:num>"
        "<m:den><m:r><m:t>b</m:t></m:r></m:den></m:f>"
        "</m:oMath></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>";
    const QString docx = buildDocxFromParts(m_dir, packageWithBody(body));
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    const QString md = norm(result.markdown);
    // $…$ delimiters survive turndown and the fraction became \frac.
    EXPECT_TRUE(md.contains('$')) << result.markdown.toStdString();
    EXPECT_TRUE(md.contains("\\frac")) << result.markdown.toStdString();
}

TEST_F(DocxImportTest, FootnoteBecomesReferenceAndDefinition)
{
    const QByteArray body =
        "<w:document " + QByteArray(kDocxW) + "><w:body>"
        "<w:p><w:r><w:t>Body text</w:t></w:r>"
        "<w:r><w:footnoteReference w:id=\"1\"/></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>";
    const QByteArray footnotes =
        "<w:footnotes xmlns:w=\"" + QByteArray("http://schemas.openxmlformats.org/wordprocessingml/2006/main") + "\">"
        "<w:footnote w:id=\"-1\"><w:p><w:r><w:t>sep</w:t></w:r></w:p></w:footnote>"
        "<w:footnote w:id=\"0\"><w:p><w:r><w:t>cont</w:t></w:r></w:p></w:footnote>"
        "<w:footnote w:id=\"1\"><w:p><w:r><w:t>Imported note</w:t></w:r></w:p></w:footnote>"
        "</w:footnotes>";
    QHash<QString, QByteArray> parts = packageWithBody(body);
    parts.insert(QStringLiteral("word/footnotes.xml"), footnotes);
    const QString docx = buildDocxFromParts(m_dir, parts);
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    const QString md = norm(result.markdown);
    EXPECT_TRUE(md.contains("[^f1]")) << result.markdown.toStdString();
    EXPECT_TRUE(md.contains("[^f1]:Importednote")) << result.markdown.toStdString();
    EXPECT_FALSE(md.contains("[^f0]")) << "sentinel footnotes must be ignored";
}

TEST_F(DocxImportTest, HeaderAndFooterBecomeFootnotes)
{
    const QByteArray body =
        "<w:document " + QByteArray(kDocxW) + "><w:body>"
        "<w:p><w:r><w:t>Body</w:t></w:r></w:p>"
        "<w:sectPr/></w:body></w:document>";
    const QByteArray rels =
        "<?xml version=\"1.0\"?><Relationships " + QByteArray(kRelNs) + ">"
        "<Relationship Id=\"rIdHdr1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" "
        "Target=\"header1.xml\"/>"
        "<Relationship Id=\"rIdFtr1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer\" "
        "Target=\"footer1.xml\"/>"
        "</Relationships>";
    const QByteArray header =
        "<w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:p><w:r><w:t>Running Head</w:t></w:r></w:p></w:hdr>";
    const QByteArray footer =
        "<w:ftr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:p><w:r><w:t>Page Info</w:t></w:r></w:p></w:ftr>";
    QHash<QString, QByteArray> parts = packageWithBody(body);
    parts.insert(QStringLiteral("word/_rels/document.xml.rels"), rels);
    parts.insert(QStringLiteral("word/header1.xml"), header);
    parts.insert(QStringLiteral("word/footer1.xml"), footer);
    const QString docx = buildDocxFromParts(m_dir, parts);
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    const QString md = norm(result.markdown);
    EXPECT_TRUE(md.contains("[^hdr1]")) << result.markdown.toStdString();
    EXPECT_TRUE(md.contains("[^hdr1]:RunningHead")) << result.markdown.toStdString();
    EXPECT_TRUE(md.contains("[^ftr1]")) << result.markdown.toStdString();
    EXPECT_TRUE(md.contains("[^ftr1]:PageInfo")) << result.markdown.toStdString();
}

TEST_F(DocxImportTest, CustomDirWritesImagesToConfiguredFolder)
{
    const QString docx = buildDocx(m_dir);
    ASSERT_FALSE(docx.isEmpty());

    const QString customDir = QDir(m_dir.path()).filePath(QStringLiteral("my-images"));
    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CustomDir;
    opts.customImageDir = customDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    EXPECT_EQ(result.writtenImages.size(), 1);
    EXPECT_TRUE(QDir(customDir).exists());
    EXPECT_TRUE(QFile::exists(result.writtenImages.first()));
    EXPECT_TRUE(result.writtenImages.first().startsWith(customDir));

    const QString md = norm(result.markdown);
    EXPECT_TRUE(md.contains("](my-images/image1.png)")) << result.markdown.toStdString();
}

TEST_F(DocxImportTest, VMergeE2EKeepsRestartCell)
{
    const QByteArray body =
        "<w:document " + QByteArray(kDocxW) + "><w:body>"
        "<w:tbl>"
        "<w:tr><w:tc><w:tcPr><w:vMerge w:val=\"restart\"/></w:tcPr>"
        "<w:p><w:r><w:t>Top</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>b1</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:tcPr><w:vMerge/></w:tcPr>"
        "<w:p><w:r><w:t>concealed</w:t></w:r></w:p></w:tc>"
        "<w:tc><w:p><w:r><w:t>b2</w:t></w:r></w:p></w:tc></w:tr>"
        "</w:tbl>"
        "<w:sectPr/></w:body></w:document>";
    const QString docx = buildDocxFromParts(m_dir, packageWithBody(body));
    ASSERT_FALSE(docx.isEmpty());

    DocxImportOptions opts;
    opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.documentDir = m_dir.path();

    DocxImportResult result = DocxImporter::import(docx, opts);
    ASSERT_TRUE(result.ok) << "import failed: " << result.error.toStdString();

    const QString md = result.markdown;
    EXPECT_TRUE(md.contains("Top")) << md.toStdString();
    EXPECT_TRUE(md.contains("b2")) << md.toStdString();
    EXPECT_FALSE(md.contains("concealed")) << "vMerge continuation text dropped";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
