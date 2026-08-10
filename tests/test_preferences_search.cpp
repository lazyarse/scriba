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
#include "PreferencesDialog.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "Preferences.h"
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
    ASSERT_EQ(pageList()->count(), 11);
    EXPECT_EQ(visiblePageCount(), 11);
    EXPECT_FALSE(infoLabel()->isVisible());
    const auto all = m_dialog->findChildren<QWidget *>();
    for (QWidget *w : all)
        EXPECT_FALSE(w->property("scribaPrefDim").toBool())
            << "unexpected dim on " << w->objectName().toUtf8().constData()
            << " / " << w->metaObject()->className();
}

TEST_F(PreferencesSearchTest, TypingNarrowsSidebarToMatchingPage)
{
    searchEdit()->setText(QStringLiteral("spelling"));
    EXPECT_TRUE(pageVisible(QStringLiteral("Spelling")));
    EXPECT_EQ(visiblePageCount(), 1);
    EXPECT_EQ(currentPage(), QStringLiteral("Spelling"));
}

TEST_F(PreferencesSearchTest, AutoSwitchesToFirstMatchingPage)
{
    searchEdit()->setText(QStringLiteral("spelling"));
    EXPECT_EQ(currentPage(), QStringLiteral("Spelling"));
    // "wrap" matches the General page's "Editor Line Wrap" group (not the
    // Editor page), so the sidebar jumps back to it.
    searchEdit()->setText(QStringLiteral("wrap"));
    EXPECT_EQ(currentPage(), QStringLiteral("General"));
}

TEST_F(PreferencesSearchTest, MultiTokenQueryMatchesSettingLabel)
{
    searchEdit()->setText(QStringLiteral("line height"));
    EXPECT_EQ(currentPage(), QStringLiteral("Editor"));
}

TEST_F(PreferencesSearchTest, PageNameIsSearchable)
{
    searchEdit()->setText(QStringLiteral("security"));
    EXPECT_TRUE(pageVisible(QStringLiteral("Security")));
    EXPECT_EQ(visiblePageCount(), 1);
}

TEST_F(PreferencesSearchTest, DimsNonMatchingWidgetsOnShownPage)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    ASSERT_EQ(currentPage(), QStringLiteral("General"));

    QLabel *wrapLabel = nullptr;
    QCheckBox *reopenCheck = nullptr;
    for (QLabel *l : m_dialog->findChildren<QLabel *>())
        if (l->text().contains(QStringLiteral("Wrap text"), Qt::CaseInsensitive))
            wrapLabel = l;
    for (QCheckBox *cb : m_dialog->findChildren<QCheckBox *>())
        if (cb->text().contains(QStringLiteral("Open last session"), Qt::CaseInsensitive))
            reopenCheck = cb;
    ASSERT_TRUE(wrapLabel);
    ASSERT_TRUE(reopenCheck);
    EXPECT_FALSE(wrapLabel->property("scribaPrefDim").toBool());
    EXPECT_TRUE(reopenCheck->property("scribaPrefDim").toBool());
}

TEST_F(PreferencesSearchTest, GroupBoxContainingMatchStaysVisible)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    QGroupBox *wrapGroup = nullptr;
    QGroupBox *autoSaveGroup = nullptr;
    for (QGroupBox *g : m_dialog->findChildren<QGroupBox *>()) {
        if (g->title().contains(QStringLiteral("Line Wrap")))
            wrapGroup = g;
        if (g->title() == QStringLiteral("Auto-Save"))
            autoSaveGroup = g;
    }
    ASSERT_TRUE(wrapGroup);
    ASSERT_TRUE(autoSaveGroup);
    EXPECT_FALSE(wrapGroup->property("scribaPrefDim").toBool());
    EXPECT_TRUE(autoSaveGroup->property("scribaPrefDim").toBool());
}

TEST_F(PreferencesSearchTest, ClearRestoresAllPagesAndDims)
{
    searchEdit()->setText(QStringLiteral("wrap"));
    searchEdit()->clear();
    EXPECT_EQ(visiblePageCount(), 11);
    EXPECT_FALSE(infoLabel()->isVisible());
    const auto all = m_dialog->findChildren<QWidget *>();
    for (QWidget *w : all)
        EXPECT_FALSE(w->property("scribaPrefDim").toBool());
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

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
