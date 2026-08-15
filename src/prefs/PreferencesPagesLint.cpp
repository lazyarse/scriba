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
#include "PreferencesDialog.h"

#include "prefs/Preferences.h"
#include "StaticHelpers.h"
#include "validation/MdLintConfig.h"
#include "validation/MdLintRules.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMetaType>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Generic generated param editor: one row per MdLintParam. QSpinBox for
// ints, QCheckBox for bools, QComboBox (editable) for strings, QListWidget
// for QStringList.
struct ParamEditor {
    QString name;
    QWidget *widget = nullptr;
    std::function<QJsonValue()> collect;
};

} // namespace

void PreferencesDialog::setupLintPage()
{
    const QSettings settings;
    const MdLintConfig cfg = MdLintConfig::fromJson(
        settings.value(Preferences::MarkdownLintConfig).toString());

    QWidget *page = addPage(tr("Markdown lint"));
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 16, 0, 0);
    outer->setSpacing(8);

    auto *hint = new QLabel(
        tr("Rules checked as you type (when \"Check markdown as you type\" is "
           "on, Proofing page). The line-format checks match markdownlint's "
           "rule catalog (MD001-MD060)."));
    hint->setWordWrap(true);
    hint->setObjectName("markdown-lint-hint");
    outer->addWidget(hint);

    auto *btnRow = new QHBoxLayout;
    auto *enableAll = new QPushButton(tr("Enable &all"));
    enableAll->setObjectName("markdown-lint-enable-all");
    auto *disableAll = new QPushButton(tr("&Disable all"));
    disableAll->setObjectName("markdown-lint-disable-all");
    auto *restore = new QPushButton(tr("&Restore defaults"));
    restore->setObjectName("markdown-lint-restore");
    btnRow->addWidget(enableAll);
    btnRow->addWidget(disableAll);
    btnRow->addWidget(restore);
    btnRow->addStretch();
    outer->addLayout(btnRow);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollInner = new QWidget;
    auto *groupsLayout = new QVBoxLayout(scrollInner);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(scrollInner);
    outer->addWidget(scroll);

    for (const QString &tag : MdLintRules::allTags()) {
        const QStringList ids = MdLintRules::rulesForTag(tag);
        if (ids.isEmpty())
            continue;
        auto *group = new QGroupBox(tag);
        group->setObjectName("markdown-lint-tag-" + tag);
        auto *gl = new QVBoxLayout(group);
        auto *allCheck = new QCheckBox(tr("Whole group"));
        allCheck->setObjectName("markdown-lint-tag-" + tag + "-all");
        allCheck->setChecked(std::all_of(ids.begin(), ids.end(),
                                         [&cfg](const QString &id) { return cfg.enabled(id); }));
        gl->addWidget(allCheck);
        for (const QString &id : ids) {
            const MdLintRule *rule = MdLintRules::byKey(id);
            if (!rule)
                continue;
            auto *row = new QHBoxLayout;
            auto *check = new QCheckBox(
                tr("%1 %2 — %3").arg(id, rule->alias, rule->description));
            check->setObjectName("mdlint-" + id);
            check->setToolTip(rule->description);
            check->setChecked(cfg.enabled(id));
            m_lintRuleChecks.append({id, check});
            row->addWidget(check);
            const auto params = MdLintRules::paramsFor(id);
            if (!params.isEmpty()) {
                auto *btn = new QPushButton(tr("…"));
                btn->setObjectName("mdlint-" + id + "-params");
                btn->setToolTip(tr("Configure %1 (%2)").arg(id, rule->alias));
                const QString ruleId = id;
                const QJsonObject saved = m_lintParams.value(ruleId).toObject();
                connect(btn, &QPushButton::clicked, this, [this, ruleId, saved] {
                    QDialog dlg(this);
                    dlg.setWindowTitle(ruleId);
                    auto *form = new QFormLayout(&dlg);
                    QVector<ParamEditor> editors;
                    for (const auto &p : MdLintRules::paramsFor(ruleId)) {
                        QJsonValue val = saved.value(QLatin1String(p.name));
                        if (val.isUndefined())
                            val = QJsonValue::fromVariant(p.def);
                        QWidget *w = nullptr;
                        std::function<QJsonValue()> collect;
                        if (p.def.userType() == QMetaType::Int) {
                            auto *sp = new QSpinBox;
                            sp->setRange(0, 100000);
                            sp->setValue(val.toInt());
                            collect = [sp] { return QJsonValue(sp->value()); };
                            w = sp;
                        } else if (p.def.userType() == QMetaType::Bool) {
                            auto *cb = new QCheckBox;
                            cb->setChecked(val.toBool());
                            collect = [cb] { return QJsonValue(cb->isChecked()); };
                            w = cb;
                        } else if (p.def.userType() == QMetaType::QStringList) {
                            auto *lw = new QListWidget;
                            for (const auto &s : val.toArray())
                                lw->addItem(s.toString());
                            collect = [lw] {
                                QJsonArray arr;
                                for (int i = 0; i < lw->count(); ++i)
                                    arr.append(lw->item(i)->text());
                                return QJsonValue(arr);
                            };
                            w = lw;
                        } else {
                            auto *cmb = new QComboBox;
                            cmb->setEditable(true);
                            cmb->addItem(val.toString());
                            collect = [cmb] { return QJsonValue(cmb->currentText()); };
                            w = cmb;
                        }
                        editors.append({QLatin1String(p.name), w, collect});
                        form->addRow(QLatin1String(p.name), w);
                    }
                    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok
                                                     | QDialogButtonBox::Cancel);
                    stripButtonIcons(box);
                    box->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
                    box->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
                    form->addRow(box);
                    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                    if (dlg.exec() == QDialog::Accepted) {
                        QJsonObject obj;
                        for (const auto &e : editors)
                            obj.insert(e.name, e.collect());
                        m_lintParams.insert(ruleId, obj);
                    }
                });
                row->addWidget(btn);
            }
            auto *sev = new QComboBox;
            sev->setObjectName("mdlint-" + id + "-severity");
            sev->addItems({tr("Error"), tr("Warning")});
            sev->setCurrentIndex(cfg.severity(id) == Severity::Warning ? 1 : 0);
            sev->setEnabled(check->isChecked());
            connect(check, &QCheckBox::toggled, sev, &QComboBox::setEnabled);
            row->addWidget(sev);
            m_lintSeverityCombos.append({id, sev});
            row->addStretch();
            gl->addLayout(row);
        }
        connect(allCheck, &QCheckBox::toggled, this, [this, ids](bool on) {
            for (const auto &id : ids)
                if (auto *w = findChild<QCheckBox *>("mdlint-" + id))
                    w->setChecked(on);
        });
        groupsLayout->addWidget(group);
    }

    connect(enableAll, &QPushButton::clicked, this, [this] {
        for (auto &pair : m_lintRuleChecks)
            pair.second->setChecked(true);
    });
    connect(disableAll, &QPushButton::clicked, this, [this] {
        for (auto &pair : m_lintRuleChecks)
            pair.second->setChecked(false);
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        const auto defs = MdLintConfig::defaults();
        for (auto &pair : m_lintRuleChecks)
            pair.second->setChecked(defs.enabled(pair.first));
        for (auto &pair : m_lintSeverityCombos)
            pair.second->setCurrentIndex(defs.severity(pair.first) == Severity::Warning ? 1 : 0);
        m_lintParams = QJsonObject();
    });
}

QString PreferencesDialog::buildLintConfigJson() const
{
    QJsonObject root;
    for (const auto &pair : m_lintRuleChecks) {
        const QString &id = pair.first;
        if (!pair.second->isChecked())
            continue;
        const auto *sev = findChild<QComboBox *>("mdlint-" + id + "-severity");
        const bool warning = sev && sev->currentIndex() == 1;
        const QJsonObject params = m_lintParams.value(id).toObject();
        if (params.isEmpty()) {
            root.insert(id, warning ? QJsonValue(QStringLiteral("warning"))
                                    : QJsonValue(true));
        } else {
            QJsonObject obj;
            obj.insert(QStringLiteral("enabled"), true);
            if (warning)
                obj.insert(QStringLiteral("severity"), QStringLiteral("warning"));
            obj.insert(QStringLiteral("params"), params);
            root.insert(id, obj);
        }
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}