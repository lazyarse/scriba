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


#include "MainWindow.h"
#include "editor/Editor.h"
#include "corpus/CorpusIndex.h"
#include "spell/GrammarChecker.h"
#include "preview/MarkdownParser.h"
#include "prefs/Preferences.h"
#include "spell/SpellChecker.h"
#include "StaticHelpers.h"
#include "spell/StoppardEngine.h"
#include "validation/ValidationReport.h"
#include "validation/ValidationReportDialog.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QSettings>
#include <QStatusBar>
#include <QTextBrowser>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <algorithm>

namespace {

// Runs the whole-document grammar pass for the Validation Report on a
// background thread. GrammarChecker (StoppardEngine) is stateless and
// thread-safe, so an instance created here is safe to call from run(). Plain
// QThread subclass (no new signals), so no moc is needed.
class ValidationReportThread : public QThread
{
public:
    ValidationReportThread(GrammarChecker *checker)
        : m_checker(checker)
    {
    }

    ~ValidationReportThread() override { delete m_checker; }

    // Set before start(); read after finished().
    QVector<ValidationReport::DocumentSource> sources;
    QVector<QList<GrammarChecker::Issue>> results;

protected:
    void run() override
    {
        results.reserve(sources.size());
        for (const auto &source : sources) {
            results.append(m_checker ? m_checker->check(source.text)
                                     : QList<GrammarChecker::Issue>());
        }
    }

private:
    GrammarChecker *m_checker = nullptr;
};

} // namespace

void MainWindow::generateValidationReport()
{
    if (m_reportInFlight)
        return;

    QVector<TabEntry> tabs;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_reportTitles.contains(i))
            continue; // never re-scan an earlier report tab
        const TabInfo &info = m_tabs[i];
        if (!info.editor)
            continue;
        QString name = info.filePath.isEmpty()
            ? tr("Untitled") : QFileInfo(info.filePath).fileName();
        if (info.dirty)
            name += QStringLiteral(" *");
        tabs.append({i, name});
    }

    ValidationReportDialog dlg(tabs, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_reportOptions = dlg.options();
    m_reportInFlight = true;

    const QSet<int> selected = dlg.selectedTabIndices();
    m_reportSources.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!selected.contains(i))
            continue;
        Editor *ed = m_tabs[i].editor;
        if (!ed)
            continue;
        ValidationReport::DocumentSource src;
        src.filePath = m_tabs[i].filePath;
        src.baseDir = m_tabs[i].filePath.isEmpty() && !m_corpus.filePath.isEmpty()
            ? m_corpus.rootDir() : QString();
        src.text = ed->toPlainText();
        m_reportSources.append(src);
    }

    QSettings settings;
    // An active corpus's dictionary overrides the global dialect/language for
    // the report; empty corpus fields fall back to the per-user preferences.
    const CorpusDictionary &dict = m_corpus.dictionary;
    const bool corpusActive = !m_corpus.filePath.isEmpty();
    const QString dialect = corpusActive && !dict.dialect.isEmpty()
        ? dict.dialect
        : settings
              .value(Preferences::GrammarDialect, QStringLiteral("American")).toString();

    // Spelling: a fresh checker honors the configured dictionary and dialect
    // regardless of which tab is active. Used only on the UI thread, and only
    // when the user asked for the spelling category.
    std::unique_ptr<SpellChecker> spellChecker;
    if (m_reportOptions.categories.contains(ValidationReport::Category::Spelling)) {
        spellChecker = std::make_unique<SpellChecker>();
        spellChecker->setDialect(dialect);
        const QString language = corpusActive && !dict.language.isEmpty()
            ? dict.language
            : settings.value(Preferences::DictionaryLanguage).toString();
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect) : language;
        if (!spellChecker->loadLanguage(resolved)) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (spellChecker->loadLanguage(lang))
                    break;
            }
        }
    }

    ValidationReport report;
    m_reportDocs = report.scan(m_reportSources,
                               spellChecker ? spellChecker.get() : nullptr,
                               m_reportOptions);

    if (m_reportOptions.categories.contains(ValidationReport::Category::Grammar)) {
        // Grammar pass on a background thread (whole-document and expensive);
        // the results are merged into m_reportDocs when the thread finishes.
        auto *worker = new ValidationReportThread(new StoppardEngine(dialect));
        worker->sources = m_reportSources;
        m_reportThread = worker;
        connect(worker, &QThread::finished, this, [this, worker]() {
            if (m_reportThread == worker)
                m_reportThread = nullptr;
            onValidationReportReady(worker->results);
            worker->deleteLater();
        });
        worker->start();
        statusBar()->showMessage(tr("Generating validation report..."));
    } else {
        openValidationReport(); // no grammar selected: assemble synchronously
    }
}

void MainWindow::onValidationReportReady(
    const QVector<QList<GrammarChecker::Issue>> &grammarIssues)
{
    const int count = qMin(m_reportDocs.size(), grammarIssues.size());
    for (int i = 0; i < count; ++i) {
        m_reportDocs[i].issues[ValidationReport::Category::Grammar] =
            ValidationReport::grammarIssuesToLineIssues(m_reportSources[i].text,
                                                        grammarIssues[i]);
    }
    openValidationReport();
}

void MainWindow::openValidationReport()
{
    m_reportInFlight = false;
    statusBar()->clearMessage();

    const QDateTime now = QDateTime::currentDateTime();
    const QString md = ValidationReport::renderMarkdown(
        m_reportDocs, now.toString(Qt::ISODate), m_reportOptions.categories);

    int idx = addTab(QString());
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    ed->setPlainText(md);
    setTabSaved(idx);
    m_reportTitles.insert(idx,
        tr("Validation Report - ") + now.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, tr("Validation report (regenerate with Ctrl+Shift+F7)"));
}

void MainWindow::openCorpusToc()
{
    if (m_corpus.documents.isEmpty()) {
        showCenteredWarning(tr("No Corpus"), tr("No corpus is open."), QString());
        return;
    }
    if (m_corpus.filePath.isEmpty()) {
        showCenteredWarning(tr("Unsaved Corpus"),
            tr("Save the corpus first — its Table of Contents is a file in the corpus folder."),
            QString());
        return;
    }

    const QString tocPath = corpusTocPath();
    if (!QFileInfo::exists(tocPath)) {
        QString links = CorpusIndex::renderTocLinks(m_corpus, tocLinks());
        QString templateText = QSettings().value(Preferences::CorpusTocTemplate).toString();
        if (templateText.trimmed().isEmpty())
            templateText = CorpusIndex::defaultTocTemplate();
        QString content = CorpusIndex::replaceTocBlock(templateText, links);
        QFile out(tocPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showCenteredWarning(tr("Table of Contents Failed"),
                tr("Could not create %1").arg(tocPath), QString());
            return;
        }
        out.write(content.toUtf8());
        out.close();
    }

    const int existing = findTabByPath(tocPath);
    if (existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
    } else {
        loadFile(tocPath);   // opens a normal, editable tab
    }
    refreshCorpusToc();
}

QHash<QString, QString> MainWindow::tocLinks() const
{
    QHash<QString, QString> links;
    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(d.path).isAbsolute())
            links.insert(abs, QUrl::fromLocalFile(abs).toString());
        else
            links.insert(abs, d.path);
    }
    return links;
}

void MainWindow::refreshCorpusToc()
{
    const QString tocPath = corpusTocPath();
    if (tocPath.isEmpty() || !QFileInfo::exists(tocPath))
        return;

    QFile in(tocPath);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString before = QString::fromUtf8(in.readAll());
    in.close();
    if (before.isEmpty() || !before.contains(CorpusIndex::tocStartMarker()))
        return; // user removed the markers: leave the file alone

    const QString after = CorpusIndex::replaceTocBlock(before, CorpusIndex::renderTocLinks(m_corpus, tocLinks()));
    if (after == before)
        return;

    QFile out(tocPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    out.write(after.toUtf8());
    out.close();

    // Sync any open tab: replace only the marker region, preserving user text
    // above/below and the dirty flag.
    const int idx = findTabByPath(tocPath);
    if (idx < 0 || !m_tabs[idx].editor)
        return;
    QTextCursor c(m_tabs[idx].editor->document());
    c.beginEditBlock();
    QTextCursor startCursor = c.document()->find(CorpusIndex::tocStartMarker(), c);
    if (!startCursor.isNull()) {
        // Select from the start-marker to just past the end-marker line.
        QTextCursor region = startCursor;
        region.setPosition(startCursor.selectionEnd(), QTextCursor::MoveAnchor);
        QTextCursor endCursor = region.document()->find(CorpusIndex::tocEndMarker(), region);
        if (!endCursor.isNull()) {
            region.setPosition(endCursor.selectionEnd(), QTextCursor::MoveAnchor);
            region.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            const QString newBlock = CorpusIndex::tocStartMarker() + QLatin1Char('\n')
                + CorpusIndex::renderTocLinks(m_corpus, tocLinks())
                + QLatin1Char('\n') + CorpusIndex::tocEndMarker();
            region.insertText(newBlock);
        }
    }
    c.endEditBlock();
    if (!m_tabs[idx].dirty)
        setTabSaved(idx);
}

void MainWindow::stopValidationReport()
{
    if (!m_reportThread)
        return;
    QThread *thread = m_reportThread;
    m_reportThread = nullptr;
    disconnect(thread, &QThread::finished, this, nullptr);
    thread->quit();
    thread->wait();
    delete thread; // ValidationReportThread dtor frees the grammar checker
}
