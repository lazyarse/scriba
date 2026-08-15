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
//
// Settings search in the Preferences dialog: typing narrows the sidebar to
// matching pages, auto-switches to the first match, dims non-matching widgets
// on the shown page (containers holding a match stay visible), shows a
// "no matches" hint, and clears everything when the box is emptied. Ctrl+F
// focuses the search box.
#include <gtest/gtest.h>
#include "prefs/PreferencesDialog.h"
#include "css/CssConfig.h"
#include "css/CssLoader.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTest>

namespace {

class PreferencesSearchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        m_config = new CssConfig;
        m_loader = new CssLoader(m_config);
        m_dialog = new PreferencesDialog(m_config, m_loader, nullptr,
            QStringLiteral("#ffffff"), QStringLiteral("#000000"));
        m_dialog->show();
    }

    void TearDown() override
    {
        delete m_dialog;
        delete m_loader;
        delete m_config;
        QSettings().clear();
    }

    QLineEdit *searchEdit() const
    {
        return m_dialog->findChild<QLineEdit *>(QStringLiteral("preferences-search"));
    }

    QListWidget *pageList() const
    {
        return m_dialog->findChild<QListWidget *>(QStringLiteral("preferences-page-list"));
    }

    QStackedWidget *pages() const
    {
        return m_dialog->findChild<QStackedWidget *>();
    }

    QLabel *infoLabel() const
    {
        return m_dialog->findChild<QLabel *>(QStringLiteral("preferences-search-info"));
    }

    bool pageVisible(const QString &name) const
    {
        for (int i = 0; i < pageList()->count(); ++i) {
            QListWidgetItem *it = pageList()->item(i);
            if (it->text() == name)
                return !it->isHidden();
        }
        return false;
    }

    int visiblePageCount() const
    {
        int n = 0;
        for (int i = 0; i < pageList()->count(); ++i)
            if (!pageList()->item(i)->isHidden())
                ++n;
        return n;
    }

    QString currentPage() const
    {
        const int idx = pages()->currentIndex();
        if (idx < 0 || idx >= pageList()->count())
            return QString();
        return pageList()->item(idx)->text();
    }

    CssConfig *m_config = nullptr;
    CssLoader *m_loader = nullptr;
    PreferencesDialog *m_dialog = nullptr;
};

TEST_F(PreferencesSearchTest, EmptySearchShowsAllPagesAndNoDim)
{
    ASSERT_EQ(pageList()->count(), 13);
    EXPECT_EQ(visiblePageCount(), 13);
    EXPECT_FALSE(infoLabel()->isVisible());
    const auto all = m_dialog->findChildren<QWidget *>();
    for (QWidget *w : all) {
        EXPECT_FALSE(w->property("scribaPrefDim").toBool())
            << "unexpected dim on " << w->objectName().toUtf8().constData()
            << " / " << w->metaObject()->className();
        EXPECT_FALSE(w->property("scribaPrefMatch").toBool())
            << "unexpected match highlight on " << w->objectName().toUtf8().constData()
            << " / " << w->metaObject()->className();
    }
}

TEST_F(PreferencesSearchTest, TypingNarrowsSidebarToMatchingPage)
{
    searchEdit()->setText(QStringLiteral("proofing"));
    EXPECT_TRUE(pageVisible(QStringLiteral("Proofing")));
    // The Markdown lint page's hint mentions the Proofing page, so it matches too.
    EXPECT_EQ(visiblePageCount(), 2);
    EXPECT_EQ(currentPage(), QStringLiteral("Proofing"));
}

TEST_F(PreferencesSearchTest, AutoSwitchesToFirstMatchingPage)
{
    searchEdit()->setText(QStringLiteral("proofing"));
    EXPECT_EQ(currentPage(), QStringLiteral("Proofing"));
    // "wrap" matches the Editor page's "Editor Line Wrap" group.
    searchEdit()->setText(QStringLiteral("wrap"));
    EXPECT_EQ(currentPage(), QStringLiteral("Editor"));
}

TEST_F(PreferencesSearchTest, MultiTokenQueryMatchesSettingLabel)
{
    searchEdit()->setText(QStringLiteral("line height"));
    EXPECT_EQ(currentPage(), QStringLiteral("Appearance"));
}

TEST_F(PreferencesSearchTest, PageNameIsSearchable)
{
    searchEdit()->setText(QStringLiteral("security"));
    EXPECT_TRUE(pageVisible(QStringLiteral("Security")));
    EXPECT_EQ(visiblePageCount(), 1);
}

TEST_F(PreferencesSearchTest, ReorganizedPageNamesAreSearchable)
{
    const QStringList queries = {
        QStringLiteral("Appearance"),
        QStringLiteral("Typesetting"),
        QStringLiteral("Metrics"),
        QStringLiteral("Auto-correct"),
        QStringLiteral("Proofing"),
    };
    for (const QString &query : queries) {
        searchEdit()->setText(query);
        EXPECT_TRUE(pageVisible(query))
            << "page not visible for query: " << query.toUtf8().constData();
        // The first sidebar match wins; the renamed page comes before any
        // other page whose widgets happen to mention the name (e.g. the
        // Themes page's "visual appearance of the editor" label).
        EXPECT_EQ(currentPage(), query)
            << "wrong current page for query: " << query.toUtf8().constData();
    }
}

TEST_F(PreferencesSearchTest, DimsNonMatchingWidgetsOnShownPage)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    ASSERT_EQ(currentPage(), QStringLiteral("Editor"));

    QLabel *wrapLabel = nullptr;
    QCheckBox *syncCheck = nullptr;
    for (QLabel *l : m_dialog->findChildren<QLabel *>())
        if (l->text().contains(QStringLiteral("Wrap text"), Qt::CaseInsensitive))
            wrapLabel = l;
    for (QCheckBox *cb : m_dialog->findChildren<QCheckBox *>())
        if (cb->text().contains(QStringLiteral("Sync editor and preview scrolling"), Qt::CaseInsensitive))
            syncCheck = cb;
    ASSERT_TRUE(wrapLabel);
    ASSERT_TRUE(syncCheck);
    EXPECT_FALSE(wrapLabel->property("scribaPrefDim").toBool());
    EXPECT_TRUE(syncCheck->property("scribaPrefDim").toBool());
    EXPECT_TRUE(wrapLabel->property("scribaPrefMatch").toBool());
    EXPECT_FALSE(syncCheck->property("scribaPrefMatch").toBool());
}

TEST_F(PreferencesSearchTest, GroupBoxContainingMatchStaysVisible)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    QGroupBox *wrapGroup = nullptr;
    QGroupBox *tablesGroup = nullptr;
    for (QGroupBox *g : m_dialog->findChildren<QGroupBox *>()) {
        if (g->title().contains(QStringLiteral("Line Wrap")))
            wrapGroup = g;
        if (g->title() == QStringLiteral("Tables"))
            tablesGroup = g;
    }
    ASSERT_TRUE(wrapGroup);
    ASSERT_TRUE(tablesGroup);
    EXPECT_FALSE(wrapGroup->property("scribaPrefDim").toBool());
    EXPECT_TRUE(tablesGroup->property("scribaPrefDim").toBool());
    EXPECT_TRUE(wrapGroup->property("scribaPrefMatch").toBool());
    EXPECT_FALSE(tablesGroup->property("scribaPrefMatch").toBool());
}

TEST_F(PreferencesSearchTest, PageNameMatchDoesNotHighlightWholePage)
{
    searchEdit()->setText(QStringLiteral("security"));
    ASSERT_EQ(currentPage(), QStringLiteral("Security"));
    int idx = -1;
    for (int i = 0; i < pageList()->count(); ++i) {
        if (pageList()->item(i)->text() == QStringLiteral("Security")) {
            idx = i;
            break;
        }
    }
    ASSERT_GE(idx, 0);
    QWidget *page = pages()->widget(idx);
    if (auto *scroll = qobject_cast<QScrollArea *>(page))
        page = scroll->widget();
    ASSERT_TRUE(page);
    EXPECT_FALSE(page->property("scribaPrefMatch").toBool());
}

TEST_F(PreferencesSearchTest, ClearRestoresAllPagesAndDims)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    searchEdit()->clear();
    EXPECT_EQ(visiblePageCount(), 13);
    EXPECT_FALSE(infoLabel()->isVisible());
    const auto all = m_dialog->findChildren<QWidget *>();
    for (QWidget *w : all) {
        EXPECT_FALSE(w->property("scribaPrefDim").toBool());
        EXPECT_FALSE(w->property("scribaPrefMatch").toBool());
    }
}

TEST_F(PreferencesSearchTest, CorpusPageHasExpectedSettingsAndStartupMoved)
{
    int corpusIdx = -1;
    for (int i = 0; i < pageList()->count(); ++i) {
        if (pageList()->item(i)->text() == QStringLiteral("Corpus")) {
            corpusIdx = i;
            break;
        }
    }
    ASSERT_GE(corpusIdx, 0);

    // Every new Corpus-page setting is searchable and routes to that page.
    const QStringList queries = {
        QStringLiteral("Open last corpus on startup"),
        QStringLiteral("Recent Corpora"),
        QStringLiteral("Monitor corpus directory"),
        QStringLiteral("When a document changes on disk"),
        QStringLiteral("When a corpus document is renamed/moved"),
        QStringLiteral("Documents outside the corpus root"),
        QStringLiteral("saving a corpus with unsaved documents"),
        QStringLiteral("File name (in the corpus folder)"),
        QStringLiteral("New-corpus template"),
    };
    for (const QString &query : queries) {
        searchEdit()->setText(query);
        EXPECT_EQ(currentPage(), QStringLiteral("Corpus"))
            << "query: " << query.toUtf8().constData();
    }

    // The startup checkbox now lives on the Corpus page, not the General page.
    auto *reopenCheck = m_dialog->findChild<QCheckBox *>(QStringLiteral("corpus-reopen-startup"));
    ASSERT_TRUE(reopenCheck);
    QWidget *corpusPageWidget = pages()->widget(corpusIdx);
    ASSERT_TRUE(corpusPageWidget);
    bool onCorpusPage = false;
    for (QWidget *p = reopenCheck->parentWidget(); p; p = p->parentWidget()) {
        if (p == corpusPageWidget) {
            onCorpusPage = true;
            break;
        }
    }
    EXPECT_TRUE(onCorpusPage);

    // The General page no longer hosts the corpus startup checkbox.
    int generalIdx = -1;
    for (int i = 0; i < pageList()->count(); ++i) {
        if (pageList()->item(i)->text() == QStringLiteral("General")) {
            generalIdx = i;
            break;
        }
    }
    ASSERT_GE(generalIdx, 0);
    QWidget *generalPageWidget = pages()->widget(generalIdx);
    ASSERT_TRUE(generalPageWidget);
    EXPECT_EQ(generalPageWidget->findChildren<QCheckBox *>(QStringLiteral("corpus-reopen-startup")).size(), 0);
}

TEST_F(PreferencesSearchTest, NoMatchShowsInfoLabel)
{
    searchEdit()->setText(QStringLiteral("zzznope"));
    EXPECT_EQ(visiblePageCount(), 0);
    EXPECT_TRUE(infoLabel()->isVisible());
    EXPECT_TRUE(infoLabel()->text().contains(QStringLiteral("No settings match")));
}

TEST_F(PreferencesSearchTest, FindShortcutFocusesSearch)
{
    QShortcut *sc = nullptr;
    for (QShortcut *s : m_dialog->findChildren<QShortcut *>()) {
        if (s->key() == QKeySequence::Find) {
            sc = s;
            break;
        }
    }
    ASSERT_TRUE(sc);
    m_dialog->activateWindow();
    m_dialog->setFocus();
    QTest::qWait(20);
    searchEdit()->clearFocus();
    QTest::qWait(20);
    EXPECT_FALSE(searchEdit()->hasFocus());
    ASSERT_TRUE(QMetaObject::invokeMethod(sc, "activated", Qt::DirectConnection));
    QTest::qWait(20);
    EXPECT_TRUE(searchEdit()->hasFocus());
}

TEST_F(PreferencesSearchTest, PrintingPagePersistsAndRestores)
{
    auto *split = m_dialog->findChild<QComboBox *>(QStringLiteral("printing-code-split"));
    auto *keepTables = m_dialog->findChild<QCheckBox *>(QStringLiteral("printing-keep-tables"));
    auto *margin = m_dialog->findChild<QLineEdit *>(QStringLiteral("printing-margin"));
    auto *size = m_dialog->findChild<QLineEdit *>(QStringLiteral("printing-size"));
    ASSERT_NE(split, nullptr);
    ASSERT_NE(keepTables, nullptr);
    ASSERT_NE(margin, nullptr);
    ASSERT_NE(size, nullptr);

    // Defaults (mirrors PrintOptions / DR-2).
    EXPECT_EQ(split->currentData().toString(), QStringLiteral("never"));
    EXPECT_TRUE(keepTables->isChecked());
    EXPECT_TRUE(margin->text().isEmpty());
    EXPECT_TRUE(size->text().isEmpty());

    // Change values, then save via OK.
    split->setCurrentIndex(split->findData(QStringLiteral("large")));
    keepTables->setChecked(false);
    margin->setText(QStringLiteral("18mm"));
    size->setText(QStringLiteral("A5"));
    auto *box = m_dialog->findChild<QDialogButtonBox *>();
    ASSERT_NE(box, nullptr);
    box->button(QDialogButtonBox::Ok)->click();
    QApplication::processEvents();

    QSettings s;
    EXPECT_EQ(s.value(Preferences::PrintCodeSplit).toString(), QStringLiteral("large"));
    EXPECT_FALSE(s.value(Preferences::PrintKeepTables).toBool());
    EXPECT_EQ(s.value(Preferences::PrintPageMargin).toString(), QStringLiteral("18mm"));
    EXPECT_EQ(s.value(Preferences::PrintPageSize).toString(), QStringLiteral("A5"));

    // Reopen the dialog and verify the page restores from settings.
    auto *d2 = new PreferencesDialog(m_config, m_loader, nullptr,
        QStringLiteral("#ffffff"), QStringLiteral("#000000"));
    d2->show();
    auto *split2 = d2->findChild<QComboBox *>(QStringLiteral("printing-code-split"));
    auto *keepTables2 = d2->findChild<QCheckBox *>(QStringLiteral("printing-keep-tables"));
    auto *margin2 = d2->findChild<QLineEdit *>(QStringLiteral("printing-margin"));
    ASSERT_NE(split2, nullptr);
    ASSERT_NE(keepTables2, nullptr);
    ASSERT_NE(margin2, nullptr);
    EXPECT_EQ(split2->currentData().toString(), QStringLiteral("large"));
    EXPECT_FALSE(keepTables2->isChecked());
    EXPECT_EQ(margin2->text(), QStringLiteral("18mm"));
    delete d2;
}

TEST_F(PreferencesSearchTest, MarkdownLintPageWidgetsExist)
{
    auto *enableAll =
        m_dialog->findChild<QPushButton *>(QStringLiteral("markdown-lint-enable-all"));
    ASSERT_NE(nullptr, enableAll);
    auto *md001 = m_dialog->findChild<QCheckBox *>(QStringLiteral("mdlint-MD001"));
    ASSERT_NE(nullptr, md001);
    EXPECT_TRUE(md001->isChecked());   // MD001 is in the scriba default set
    auto *md003 = m_dialog->findChild<QCheckBox *>(QStringLiteral("mdlint-MD003"));
    ASSERT_NE(nullptr, md003);
    EXPECT_FALSE(md003->isChecked());  // MD003 is aggressive (default off)
    auto *tagGroup = m_dialog->findChild<QGroupBox *>(QStringLiteral("markdown-lint-tag-headings"));
    ASSERT_NE(nullptr, tagGroup);
}

TEST_F(PreferencesSearchTest, LintSeverityComboDefaultsAndPersists)
{
    auto *md013 = m_dialog->findChild<QCheckBox *>(QStringLiteral("mdlint-MD013"));
    auto *sev = m_dialog->findChild<QComboBox *>(QStringLiteral("mdlint-MD013-severity"));
    ASSERT_NE(nullptr, md013);
    ASSERT_NE(nullptr, sev);
    EXPECT_TRUE(md013->isChecked());
    EXPECT_EQ(sev->currentIndex(), 1) << "MD013 is a style rule: defaults to Warning";
    EXPECT_TRUE(sev->isEnabled()) << "severity combo enabled while the rule is on";

    sev->setCurrentIndex(0); // Error
    auto *box = m_dialog->findChild<QDialogButtonBox *>();
    ASSERT_NE(box, nullptr);
    box->button(QDialogButtonBox::Ok)->click();
    QApplication::processEvents();

    const QString saved = QSettings().value(Preferences::MarkdownLintConfig).toString();
    EXPECT_TRUE(saved.contains(QStringLiteral("\"MD013\":true")))
        << "Error severity serializes as a plain true: " << saved.toStdString();

    // A rule switched to Warning serializes with an explicit severity.
    auto *d2 = new PreferencesDialog(m_config, m_loader, nullptr,
        QStringLiteral("#ffffff"), QStringLiteral("#000000"));
    d2->show();
    auto *sev2 = d2->findChild<QComboBox *>(QStringLiteral("mdlint-MD013-severity"));
    ASSERT_NE(nullptr, sev2);
    sev2->setCurrentIndex(1);
    auto *box2 = d2->findChild<QDialogButtonBox *>();
    ASSERT_NE(box2, nullptr);
    box2->button(QDialogButtonBox::Ok)->click();
    QApplication::processEvents();
    const QString saved2 = QSettings().value(Preferences::MarkdownLintConfig).toString();
    EXPECT_TRUE(saved2.contains(QStringLiteral("\"MD013\":\"warning\"")))
        << "Warning severity serializes as \"warning\": " << saved2.toStdString();
    delete d2;
}

TEST_F(PreferencesSearchTest, LintSeverityComboFollowsRuleAndRestore)
{
    auto *md003 = m_dialog->findChild<QCheckBox *>(QStringLiteral("mdlint-MD003"));
    auto *sev = m_dialog->findChild<QComboBox *>(QStringLiteral("mdlint-MD003-severity"));
    ASSERT_NE(nullptr, md003);
    ASSERT_NE(nullptr, sev);
    EXPECT_FALSE(md003->isChecked());
    EXPECT_FALSE(sev->isEnabled()) << "severity combo greyed out while the rule is off";

    md003->setChecked(true);
    EXPECT_TRUE(sev->isEnabled());
    md003->setChecked(false);
    EXPECT_FALSE(sev->isEnabled());

    // Restore defaults resets the combos to the default severities.
    auto *restore = m_dialog->findChild<QPushButton *>(QStringLiteral("markdown-lint-restore"));
    ASSERT_NE(nullptr, restore);
    sev->setCurrentIndex(0);
    restore->click();
    EXPECT_EQ(sev->currentIndex(), 0) << "restore resets MD003's combo to its default (Error)";
    auto *md009 = m_dialog->findChild<QComboBox *>(QStringLiteral("mdlint-MD009-severity"));
    ASSERT_NE(nullptr, md009);
    EXPECT_EQ(md009->currentIndex(), 1) << "MD009 defaults to Warning";
    auto *md001 = m_dialog->findChild<QComboBox *>(QStringLiteral("mdlint-MD001-severity"));
    ASSERT_NE(nullptr, md001);
    EXPECT_EQ(md001->currentIndex(), 0) << "MD001 defaults to Error";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
