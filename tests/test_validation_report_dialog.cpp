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
#include <QPushButton>
#include <QSettings>

#include "ValidationReport.h"
#include "ValidationReportDialog.h"

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
};

TEST_F(ValidationReportDialogTest, DefaultsToAllChecksSelected)
{
    ValidationReportDialog dlg;
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
    ValidationReportDialog dlg;
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
    ValidationReportDialog dlg;
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
    ValidationReportDialog dlg;
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
    ValidationReportDialog dlg;
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
        ValidationReportDialog dlg;
        auto *grammar = findCheck(dlg, QStringLiteral("&Grammar"));
        ASSERT_NE(grammar, nullptr);
        grammar->setChecked(false);
        auto *sub = findCheck(dlg, QStringLiteral("&Duplicate headings"));
        ASSERT_NE(sub, nullptr);
        sub->setChecked(false);
        dlg.accept();
    }

    ValidationReportDialog dlg;
    const auto opts = dlg.options();
    EXPECT_FALSE(opts.categories.contains(ValidationReport::Category::Grammar));
    EXPECT_FALSE(opts.markdown.contains(ValidationReport::MarkdownCheck::DuplicateHeading));
    EXPECT_TRUE(opts.markdown.contains(ValidationReport::MarkdownCheck::TrailingWhitespace));
    EXPECT_TRUE(opts.categories.contains(ValidationReport::Category::Spelling));
}

} // namespace
