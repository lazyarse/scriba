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
#include "ValidationReportDialog.h"

#include "StaticHelpers.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace {

// Each markdown sub-check: enum value, the persisted settings key and the
// checkbox label. Order must match MarkdownCheck.
struct MarkdownCheckDef {
    ValidationReport::MarkdownCheck check;
    const char *key;
    const char *label;
};

constexpr std::array<MarkdownCheckDef, 7> kMarkdownChecks = {{
    {ValidationReport::MarkdownCheck::HeadingLevelSkip, "md-heading-skip",
     "Heading level &skips"},
    {ValidationReport::MarkdownCheck::DuplicateHeading, "md-duplicate-heading",
     "&Duplicate headings"},
    {ValidationReport::MarkdownCheck::TrailingWhitespace, "md-trailing-whitespace",
     "Trailing &whitespace"},
    {ValidationReport::MarkdownCheck::ConsecutiveBlankLines, "md-blank-lines",
     "Consecutive &blank lines"},
    {ValidationReport::MarkdownCheck::OverlongLine, "md-overlong-line",
     "Lines &over 120 characters"},
    {ValidationReport::MarkdownCheck::HashNoSpace, "md-hash-no-space",
     "'#' headings without a &space"},
    {ValidationReport::MarkdownCheck::FootnoteReference, "md-footnote",
     "&Unmatched footnote references"},
}};

const char *kSettingsKey = "ValidationReport/Checks";

} // namespace

ValidationReportDialog::ValidationReportDialog(const QVector<TabEntry> &tabs, QWidget *parent)
    : QDialog(parent)
    , m_tabs(tabs)
{
    setWindowTitle(tr("Validation Report Options"));
    buildUi();
    load();
    updateMarkdownMaster(m_markdown->isChecked());
    updateButtons();
}

QSet<int> ValidationReportDialog::selectedTabIndices() const
{
    QSet<int> selected;
    for (int i = 0; i < m_tabsList->count(); ++i) {
        auto *item = m_tabsList->item(i);
        if (item->checkState() == Qt::Checked)
            selected.insert(item->data(Qt::UserRole).toInt());
    }
    return selected;
}

ValidationReport::ValidationOptions ValidationReportDialog::options() const
{
    ValidationReport::ValidationOptions opts;
    opts.categories.clear(); // start empty; checked boxes opt in
    if (m_spelling->isChecked())
        opts.categories.insert(ValidationReport::Category::Spelling);
    if (m_grammar->isChecked())
        opts.categories.insert(ValidationReport::Category::Grammar);
    if (m_links->isChecked())
        opts.categories.insert(ValidationReport::Category::Links);
    if (m_markdown->isChecked())
        opts.categories.insert(ValidationReport::Category::Markdown);

    opts.markdown.clear();
    if (m_markdown->isChecked()) {
        for (qsizetype i = 0; i < m_markdownChecks.size(); ++i)
            if (m_markdownChecks.at(i)->isChecked())
                opts.markdown.insert(kMarkdownChecks[static_cast<size_t>(i)].check);
    }
    return opts;
}

void ValidationReportDialog::accept()
{
    QStringList saved;
    if (m_spelling->isChecked()) saved << QStringLiteral("spelling");
    if (m_grammar->isChecked()) saved << QStringLiteral("grammar");
    if (m_links->isChecked()) saved << QStringLiteral("links");
    if (m_markdown->isChecked()) saved << QStringLiteral("markdown");
    for (const auto &def : kMarkdownChecks)
        if (m_markdownChecks.at(static_cast<qsizetype>(def.check))->isChecked())
            saved << QLatin1String(def.key);
    QSettings().setValue(QLatin1String(kSettingsKey), saved);
    QDialog::accept();
}

void ValidationReportDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *info = new QLabel(
        tr("Choose which documents to scan and which checks to run, then press Generate."), this);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto *tabRow = new QHBoxLayout;
    tabRow->addWidget(new QLabel(tr("&Documents to scan"), this));
    tabRow->addStretch();
    auto *selectAllBtn = new QPushButton(tr("Select &All"), this);
    auto *selectNoneBtn = new QPushButton(tr("&None"), this);
    stripButtonIcon(selectAllBtn);
    stripButtonIcon(selectNoneBtn);
    tabRow->addWidget(selectAllBtn);
    tabRow->addWidget(selectNoneBtn);
    layout->addLayout(tabRow);

    m_tabsList = new QListWidget(this);
    m_tabsList->setMaximumHeight(180);
    for (const TabEntry &tab : m_tabs) {
        auto *item = new QListWidgetItem(tab.label, m_tabsList);
        item->setData(Qt::UserRole, tab.index);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    layout->addWidget(m_tabsList);

    connect(selectAllBtn, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_tabsList->count(); ++i)
            m_tabsList->item(i)->setCheckState(Qt::Checked);
        updateButtons();
    });
    connect(selectNoneBtn, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_tabsList->count(); ++i)
            m_tabsList->item(i)->setCheckState(Qt::Unchecked);
        updateButtons();
    });
    connect(m_tabsList, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *) { updateButtons(); });

    m_spelling = new QCheckBox(tr("Sp&elling"), this);
    m_grammar = new QCheckBox(tr("&Grammar"), this);
    m_links = new QCheckBox(tr("&Links && anchors"), this);
    m_markdown = new QCheckBox(tr("&Markdown consistency"), this);

    layout->addWidget(m_spelling);
    layout->addWidget(m_grammar);
    layout->addWidget(m_links);
    layout->addWidget(m_markdown);

    auto *subLayout = new QVBoxLayout;
    subLayout->setContentsMargins(24, 0, 0, 0);
    for (const auto &def : kMarkdownChecks) {
        auto *check = new QCheckBox(tr(def.label), this);
        m_markdownChecks.append(check);
        subLayout->addWidget(check);
    }
    layout->addLayout(subLayout);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Generate"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(m_buttonBox);
    layout->addWidget(m_buttonBox);

    connect(m_spelling, &QCheckBox::toggled, this, &ValidationReportDialog::updateButtons);
    connect(m_grammar, &QCheckBox::toggled, this, &ValidationReportDialog::updateButtons);
    connect(m_links, &QCheckBox::toggled, this, &ValidationReportDialog::updateButtons);
    connect(m_markdown, &QCheckBox::toggled, this, &ValidationReportDialog::updateMarkdownMaster);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ValidationReportDialog::load()
{
    const QStringList saved = QSettings().value(QLatin1String(kSettingsKey)).toStringList();
    if (saved.isEmpty()) {
        m_spelling->setChecked(true);
        m_grammar->setChecked(true);
        m_links->setChecked(true);
        m_markdown->setChecked(true);
        for (auto *check : m_markdownChecks)
            check->setChecked(true);
        return;
    }
    auto enabled = [&saved](const char *key) {
        return saved.contains(QLatin1String(key));
    };
    m_spelling->setChecked(enabled("spelling"));
    m_grammar->setChecked(enabled("grammar"));
    m_links->setChecked(enabled("links"));
    m_markdown->setChecked(enabled("markdown"));
    for (qsizetype i = 0; i < m_markdownChecks.size(); ++i)
        m_markdownChecks.at(i)->setChecked(enabled(kMarkdownChecks[static_cast<size_t>(i)].key));
}

void ValidationReportDialog::updateButtons()
{
    const bool anyCategory = m_spelling->isChecked() || m_grammar->isChecked()
        || m_links->isChecked()
        || (m_markdown->isChecked()
            && std::any_of(m_markdownChecks.begin(), m_markdownChecks.end(),
                           [](const QCheckBox *c) { return c->isChecked(); }));
    bool anyTab = false;
    for (int i = 0; i < m_tabsList->count(); ++i)
        if (m_tabsList->item(i)->checkState() == Qt::Checked)
            anyTab = true;
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(anyCategory && anyTab);
}

void ValidationReportDialog::updateMarkdownMaster(bool checked)
{
    for (auto *check : m_markdownChecks)
        check->setEnabled(checked);
    updateButtons();
}
