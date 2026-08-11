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
// End-to-end corpus export: a real MainWindow drives ExportCorpusDialog and
// the export pipeline (per-document rendering + index page + optional zip).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <functional>
#include <memory>

#include "Corpus.h"
#include "Editor.h"
#include "ExportCorpusDialog.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

void writeFile(const QString &path, const QString &content)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    f.write(content.toUtf8());
    f.close();
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

bool waitFor(const std::function<bool()> &cond, int timeoutMs = 30000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QTest::qWait(50);
        if (cond())
            return true;
    }
    QApplication::processEvents();
    return cond();
}

class CorpusExportTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_corpus_export";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override
    {
        QSettings s;
        s.clear();
        s.setValue(Preferences::ReopenLastCorpus, false);
        s.setValue(Preferences::AutoSaveOnExit, false);
        s.setValue(Preferences::AutoSaveInterval, 0);
        s.setValue(Preferences::CorpusExternalExportDirName, QStringLiteral("external"));

        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_ext.reset(new QTemporaryDir);
        ASSERT_TRUE(m_ext->isValid());
        m_out.reset(new QTemporaryDir);
        ASSERT_TRUE(m_out->isValid());
        m_root = m_dir->path();
        m_corpusPath = m_root + "/corpus.scriba";
        m_outputDir = m_out->path();
    }

    void TearDown() override
    {
        delete m_window;
        m_window = nullptr;
    }

    void makeCorpus()
    {
        ASSERT_TRUE(QDir(m_root).mkpath("docs"));
        writeFile(m_root + "/docs/a.md", "# Alpha\n\n## Sub A\n\nText.\n");
        writeFile(m_root + "/b.md", "# Beta\n");
        writeFile(m_ext->path() + "/ext.md", "# Ext\n");

        Corpus c;
        c.filePath = m_corpusPath;
        c.name = QStringLiteral("My Corpus");
        c.monitor = true;
        c.active = 0;
        c.documents = {
            CorpusDocument{ .path = QStringLiteral("docs/a.md") },
            CorpusDocument{ .path = QStringLiteral("b.md") },
            CorpusDocument{ .path = m_ext->path() + "/ext.md" },
            CorpusDocument{ .path = QString(), .content = QStringLiteral("# Embedded\n") },
        };
        ASSERT_TRUE(c.save());
    }

    void openCorpus()
    {
        m_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
        QApplication::processEvents();
        m_window->openCorpusFile(m_corpusPath, /*skipPrompt=*/true);
        QApplication::processEvents();
        QTest::qWait(200);      // let the watcher settle on the initial paths
    }

    // Drives the modal ExportCorpusDialog: picks the format radio button,
    // sets the output directory, optionally enables the zip checkbox and the
    // external-docs checkbox, then accepts.
    void driveDialog(ExportCorpusDialog::Format format, bool zip, bool external)
    {
        auto *dlg = qobject_cast<ExportCorpusDialog *>(qApp->activeModalWidget());
        ASSERT_NE(dlg, nullptr) << "ExportCorpusDialog must be the active modal widget";

        for (QRadioButton *rb : dlg->findChildren<QRadioButton *>()) {
            const bool want = (format == ExportCorpusDialog::Format::Html && rb->text().contains("HTML"))
                           || (format == ExportCorpusDialog::Format::Docx && rb->text().contains("Word"))
                           || (format == ExportCorpusDialog::Format::Pdf && rb->text().contains("PDF"));
            if (want) {
                rb->setChecked(true);
                break;
            }
        }

        for (QLineEdit *le : dlg->findChildren<QLineEdit *>()) {
            if (le->isReadOnly())
                le->setText(m_outputDir);
        }
        for (QCheckBox *cb : dlg->findChildren<QCheckBox *>()) {
            if (cb->text().contains("zip"))
                cb->setChecked(zip);
            else if (cb->text().contains("outside"))
                cb->setChecked(external);
        }

        auto *btnBox = dlg->findChild<QDialogButtonBox *>();
        ASSERT_NE(btnBox, nullptr);
        auto *okBtn = btnBox->button(QDialogButtonBox::Ok);
        ASSERT_NE(okBtn, nullptr);
        okBtn->click();
    }

    void runExport(ExportCorpusDialog::Format format, bool zip = false, bool external = true)
    {
        QTimer::singleShot(0, [this, format, zip, external]() {
            driveDialog(format, zip, external);
        });
        const auto actions = m_window->findChildren<QAction *>();
        bool triggered = false;
        for (QAction *a : actions) {
            if (a->text().contains(QStringLiteral("Export Corpus"))) {
                a->trigger();
                triggered = true;
                break;
            }
        }
        ASSERT_TRUE(triggered) << "Export Corpus action must exist";
        // The action triggers a modal exec(); the singleShot lambda drives it.
        ASSERT_TRUE(waitFor([&] { return qApp->activeModalWidget() == nullptr; }))
            << "exportCorpus() must return (dialog closed)";
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QTemporaryDir> m_ext;
    std::unique_ptr<QTemporaryDir> m_out;
    QString m_root;
    QString m_corpusPath;
    QString m_outputDir;
    MainWindow *m_window = nullptr;
};

TEST_F(CorpusExportTest, ExportHtmlWritesPagesIndexAndZip)
{
    makeCorpus();
    openCorpus();
    runExport(ExportCorpusDialog::Format::Html, /*zip=*/true);

    // In-root documents keep their relative path.
    EXPECT_TRUE(QFile::exists(m_outputDir + "/docs/a.md.html"));
    EXPECT_TRUE(QFile::exists(m_outputDir + "/b.md.html"));

    // External document mirrors into the named subfolder.
    EXPECT_TRUE(QFile::exists(m_outputDir + "/external/ext.md.html"));

    // Embedded document gets an Untitled-N file.
    EXPECT_TRUE(QFile::exists(m_outputDir + "/Untitled-1.html"));

    // Index page links the exported pages.
    const QString index = readFile(m_outputDir + "/index.html");
    EXPECT_FALSE(index.isEmpty());
    EXPECT_TRUE(index.contains("My Corpus"));
    EXPECT_TRUE(index.contains("docs/a.md.html"));
    EXPECT_TRUE(index.contains("b.md.html"));
    EXPECT_TRUE(index.contains("external/ext.md.html"));
    EXPECT_TRUE(index.contains("Untitled-1.html"));
    EXPECT_TRUE(index.contains("#alpha"));
    EXPECT_TRUE(index.contains("#sub-a"));

    // Rendered pages carry their headings.
    const QString alpha = readFile(m_outputDir + "/docs/a.md.html");
    EXPECT_TRUE(alpha.contains("<h1"));
    EXPECT_TRUE(alpha.contains("Alpha"));

    // Zip archive of the whole output directory.
    EXPECT_TRUE(QFile::exists(m_outputDir + "/My Corpus.zip"));
}

TEST_F(CorpusExportTest, ExportDocxWritesPagesAndDocxIndex)
{
    makeCorpus();
    openCorpus();
    runExport(ExportCorpusDialog::Format::Docx, /*zip=*/false, /*external=*/false);

    EXPECT_TRUE(QFile::exists(m_outputDir + "/docs/a.md.docx"));
    EXPECT_TRUE(QFile::exists(m_outputDir + "/b.md.docx"));
    // External export disabled -> skipped, no external/ subfolder.
    EXPECT_FALSE(QFile::exists(m_outputDir + "/external/ext.md.docx"));

    // DOCX files are zip containers; index lives in files/.
    EXPECT_TRUE(QFile::exists(m_outputDir + "/files/My Corpus-index.docx"));
    const QString indexPath = m_outputDir + "/files/My Corpus-index.docx";
    QFile f(indexPath);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const QByteArray data = f.readAll();
    f.close();
    EXPECT_TRUE(data.startsWith("PK"));
}

TEST_F(CorpusExportTest, ExternalExportCanBeDisabled)
{
    makeCorpus();
    openCorpus();
    runExport(ExportCorpusDialog::Format::Html, /*zip=*/false, /*external=*/false);

    EXPECT_TRUE(QFile::exists(m_outputDir + "/docs/a.md.html"));
    EXPECT_TRUE(QFile::exists(m_outputDir + "/b.md.html"));
    EXPECT_FALSE(QFile::exists(m_outputDir + "/external/ext.md.html"));

    const QString index = readFile(m_outputDir + "/index.html");
    EXPECT_FALSE(index.contains("ext.md.html"));
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
