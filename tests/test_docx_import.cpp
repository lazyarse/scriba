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
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "DocxExporter.h"
#include "DocxImporter.h"
#include "TestConfig.h"

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

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
