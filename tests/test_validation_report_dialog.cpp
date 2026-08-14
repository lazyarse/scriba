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

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>

#include "validation/ValidationReport.h"
#include "validation/ValidationReportDialog.h"

namespace {

class ValidationReportDialogTest : public ::testing::Test
{
protected:
    void SetUp() override { QSettings().clear(); }

    QCheckBox *findCheck(ValidationReportDialog &dlg, const QString &text) const
    {
        const auto boxes = dlg.findChildren<QCheckBox*>();
        for (auto *box : boxes)
            if (box->text() == text)
                return box;
        return nullptr;
    }

    QPushButton *okButton(ValidationReportDialog &dlg) const
    {
        auto *box = dlg.findChild<QDialogButtonBox*>();
        return box->button(QDialogButtonBox::Ok);
    }

    QPushButton *findButton(ValidationReportDialog &dlg, const QString &text) const
    {
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto *btn : buttons)
            if (btn->text() == text)
                return btn;
        return nullptr;
    }

    QListWidget *tabsList(ValidationReportDialog &dlg) const
    {
        return dlg.findChild<QListWidget*>();
    }

    // Two open, scannable tabs: index 0 ("a.md") and index 1 ("b.md").
    QVector<TabEntry> twoTabs() const
    {
        return {{0, QStringLiteral("a.md")}, {1, QStringLiteral("b.md")}};
    }
};

TEST_F(ValidationReportDialogTest, DefaultsToAllChecksSelected)
{
    ValidationReportDialog dlg(twoTabs());
    const auto opts = dlg.options();
    EXPECT_EQ(4, opts.categories.size());
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Spelling));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Grammar));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Links));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Markdown));
    EXPECT_EQ(7, opts.markdown.size());
    EXPECT_TRUE(okButton(dlg)->isEnabled());
}

TEST_F(ValidationReportDialogTest, TogglingOffRemovesCategory)
{
    ValidationReportDialog dlg(twoTabs());
    auto *spelling = findCheck(dlg, QStringLiteral("Sp&elling"));
    ASSERT_NE(spelling, nullptr);
    spelling->setChecked(false);
    const auto opts = dlg.options();
    EXPECT_FALSE(opts.categories.contains(ValidationReport::Category::Spelling));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Grammar));
    EXPECT_EQ(7, opts.markdown.size());
}

TEST_F(ValidationReportDialogTest, MarkdownMasterGatesSubChecks)
{
    ValidationReportDialog dlg(twoTabs());
    auto *markdown = findCheck(dlg, QStringLiteral("&Markdown consistency"));
    ASSERT_NE(markdown, nullptr);
    auto *sub = findCheck(dlg, QStringLiteral("&Duplicate headings"));
    ASSERT_NE(sub, nullptr);

    markdown->setChecked(false);
    EXPECT_FALSE(sub->isEnabled());
    const auto opts = dlg.options();
    EXPECT_FALSE(opts.categories.contains(ValidationReport::Category::Markdown));
    EXPECT_TRUE(opts.markdown.isEmpty());

    markdown->setChecked(true);
    EXPECT_TRUE(sub->isEnabled());
    EXPECT_TRUE(dlg.options().categories.contains(ValidationReport::Category::Markdown));
}

TEST_F(ValidationReportDialogTest, SubCheckToggleShowsInOptions)
{
    ValidationReportDialog dlg(twoTabs());
    auto *sub = findCheck(dlg, QStringLiteral("&Unmatched footnote references"));
    ASSERT_NE(sub, nullptr);
    sub->setChecked(false);
    const auto opts = dlg.options();
    EXPECT_FALSE(opts.markdown.contains(ValidationReport::MarkdownCheck::FootnoteReference));
    EXPECT_EQ(6, opts.markdown.size());
    EXPECT_TRUE(opts.markdown.contains(ValidationReport::MarkdownCheck::DuplicateHeading));
}

TEST_F(ValidationReportDialogTest, OkDisabledWhenNothingSelected)
{
    ValidationReportDialog dlg(twoTabs());
    for (auto *check : dlg.findChildren<QCheckBox*>())
        check->setChecked(false);
    EXPECT_FALSE(okButton(dlg)->isEnabled());

    // Markdown alone with no sub-check selected is still "nothing to run".
    auto *markdown = findCheck(dlg, QStringLiteral("&Markdown consistency"));
    markdown->setChecked(true);
    EXPECT_FALSE(okButton(dlg)->isEnabled());
}

TEST_F(ValidationReportDialogTest, SelectionPersistsAcrossDialogInstances)
{
    {
        ValidationReportDialog dlg(twoTabs());
        auto *grammar = findCheck(dlg, QStringLiteral("&Grammar"));
        ASSERT_NE(grammar, nullptr);
        grammar->setChecked(false);
        auto *sub = findCheck(dlg, QStringLiteral("&Duplicate headings"));
        ASSERT_NE(sub, nullptr);
        sub->setChecked(false);
        dlg.accept();
    }

    ValidationReportDialog dlg(twoTabs());
    const auto opts = dlg.options();
    EXPECT_FALSE(opts.categories.contains(ValidationReport::Category::Grammar));
    EXPECT_FALSE(opts.markdown.contains(ValidationReport::MarkdownCheck::DuplicateHeading));
    EXPECT_TRUE(opts.markdown.contains(ValidationReport::MarkdownCheck::TrailingWhitespace));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Spelling));
}

TEST_F(ValidationReportDialogTest, AllTabsSelectedByDefault)
{
    ValidationReportDialog dlg(twoTabs());
    ASSERT_NE(tabsList(dlg), nullptr);
    EXPECT_EQ(2, tabsList(dlg)->count());
    const auto selected = dlg.selectedTabIndices();
    EXPECT_TRUE(selected.contains(0));
    EXPECT_TRUE(selected.contains(1));
    EXPECT_EQ(2, selected.size());
}

TEST_F(ValidationReportDialogTest, UncheckingTabExcludesIt)
{
    ValidationReportDialog dlg(twoTabs());
    auto *lst = tabsList(dlg);
    ASSERT_NE(lst, nullptr);
    lst->item(1)->setCheckState(Qt::Unchecked);
    const auto selected = dlg.selectedTabIndices();
    EXPECT_TRUE(selected.contains(0));
    EXPECT_EQ(1, selected.size());
    EXPECT_FALSE(selected.contains(1));
}

TEST_F(ValidationReportDialogTest, OkDisabledWhenNoTabSelected)
{
    ValidationReportDialog dlg(twoTabs());
    auto *none = findButton(dlg, QStringLiteral("&None"));
    ASSERT_NE(none, nullptr);
    none->click();
    // All categories remain selected, but with no document to scan the
    // Generate button must be disabled.
    EXPECT_TRUE(dlg.selectedTabIndices().isEmpty());
    EXPECT_FALSE(okButton(dlg)->isEnabled());
}

TEST_F(ValidationReportDialogTest, SelectAllRestoresChecklist)
{
    ValidationReportDialog dlg(twoTabs());
    auto *none = findButton(dlg, QStringLiteral("&None"));
    auto *all = findButton(dlg, QStringLiteral("Select &All"));
    ASSERT_NE(none, nullptr);
    ASSERT_NE(all, nullptr);
    none->click();
    EXPECT_EQ(0, dlg.selectedTabIndices().size());
    EXPECT_FALSE(okButton(dlg)->isEnabled());
    all->click();
    EXPECT_EQ(2, dlg.selectedTabIndices().size());
    EXPECT_TRUE(okButton(dlg)->isEnabled());
}

TEST_F(ValidationReportDialogTest, UntitledTabShown) {
    ValidationReportDialog dlg({{0, QStringLiteral("Untitled")}});
    auto *lst = tabsList(dlg);
    ASSERT_NE(lst, nullptr);
    EXPECT_EQ(1, lst->count());
    EXPECT_EQ(QStringLiteral("Untitled"), lst->item(0)->text());
    EXPECT_TRUE(dlg.selectedTabIndices().contains(0));
}

} // namespace
