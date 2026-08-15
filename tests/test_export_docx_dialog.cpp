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
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSettings>
#include <QDir>
#include "io/ExportDocxDialog.h"
#include "TestConfig.h"

TEST(ExportDocxDialogTest, DefaultsToNoTemplate)
{
    ExportDocxDialog dlg(QStringLiteral("h1 { color: #123456; }"));
    EXPECT_TRUE(dlg.templatePath().isEmpty())
        << "templatePath must be empty by default";
}

TEST(ExportDocxDialogTest, TemplateSelectionDisablesPageLayout)
{
    ExportDocxDialog dlg{QString()};
    auto *useTpl = dlg.findChild<QCheckBox *>(QStringLiteral("useTemplateCheck"));
    auto *landscape = dlg.findChild<QCheckBox *>(QStringLiteral("landscapeCheck"));
    auto *top = dlg.findChild<QDoubleSpinBox *>(QStringLiteral("marginTopSpin"));
    auto *pageNums = dlg.findChild<QCheckBox *>(QStringLiteral("pageNumbersCheck"));
    ASSERT_TRUE(useTpl) << "useTemplateCheck must exist";
    ASSERT_TRUE(landscape && top && pageNums) << "page-layout controls must exist";

    EXPECT_TRUE(landscape->isEnabled());
    EXPECT_TRUE(top->isEnabled());

    useTpl->setChecked(true);
    EXPECT_FALSE(landscape->isEnabled())
        << "template selection must disable landscape (template owns page setup)";
    EXPECT_FALSE(top->isEnabled());
    EXPECT_FALSE(pageNums->isEnabled());

    useTpl->setChecked(false);
    EXPECT_TRUE(landscape->isEnabled())
        << "clearing template selection must re-enable page-layout controls";
}

TEST(ExportDocxDialogTest, TemplatePathPersistsOnAccept)
{
    ExportDocxDialog dlg{QString()};
    auto *useTpl = dlg.findChild<QCheckBox *>(QStringLiteral("useTemplateCheck"));
    auto *edit = dlg.findChild<QLineEdit *>(QStringLiteral("templatePathEdit"));
    ASSERT_TRUE(useTpl && edit);

    const QString tpl = QDir::tempPath() + QLatin1String("/my-template.docx");
    useTpl->setChecked(true);
    edit->setText(tpl);
    dlg.accept();

    EXPECT_EQ(dlg.templatePath(), tpl);
    QSettings s;
    EXPECT_EQ(s.value(QStringLiteral("DocxExport/Template")).toString(), tpl);
    EXPECT_TRUE(s.value(QStringLiteral("DocxExport/UseTemplate")).toBool());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}