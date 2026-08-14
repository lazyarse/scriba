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
        m_reportSources.append({m_tabs[i].filePath, ed->toPlainText()});
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

void MainWindow::viewTableOfContents()
{
    if (m_corpus.documents.isEmpty()) {
        showCenteredWarning(tr("No Corpus"), tr("No corpus is open."), QString());
        return;
    }
    const QString md = renderTocMarkdown();

    // Refresh an existing TOC tab in place and bring it to the front.
    if (!m_tocTabs.isEmpty()) {
        refreshOpenToc();
        for (auto it = m_tocTabs.begin(); it != m_tocTabs.end(); ++it) {
            if (it.key() < m_tabs.size() && m_tabs[it.key()].editor) {
                m_tabBar->setCurrentIndex(it.key());
                break;
            }
        }
        return;
    }

    const int idx = addTab(QString());
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    ed->setReadOnly(true);
    ed->setPlainText(md);
    setTabSaved(idx);
    m_tocTabs.insert(idx, QStringLiteral("📑 Table of Contents"));
    updateTabLabel(idx);
    m_tabBar->setCurrentIndex(idx);
    refreshPreviewForTocTab(idx, m_corpus.rootDir());
}

QString MainWindow::renderTocMarkdown() const
{
    QHash<QString, QString> links;
    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(d.path).isAbsolute())
            links.insert(abs, QUrl::fromLocalFile(abs).toString());   // out-of-root: absolute URL
        else
            links.insert(abs, d.path);                                 // in-root: relative to the corpus root
    }
    return CorpusIndex::renderToc(m_corpus, links);
}

void MainWindow::refreshOpenToc()
{
    const QString md = renderTocMarkdown();
    for (auto it = m_tocTabs.begin(); it != m_tocTabs.end(); ++it) {
        if (it.key() >= m_tabs.size() || m_tabs[it.key()].editor == nullptr)
            continue;
        m_tabs[it.key()].editor->setPlainText(md);
        setTabSaved(it.key());
        if (m_tabBar->currentIndex() == it.key())
            refreshPreviewForTocTab(it.key(), m_corpus.rootDir());
    }
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
