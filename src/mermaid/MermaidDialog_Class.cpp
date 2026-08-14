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


#include "MermaidDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMap>
#include <QTableWidget>

void MermaidDialog::saveCurrentClassData()
{
    if (m_lastClassRow < 0) return;
    ClassData data;
    for (int r = 0; r < m_classFieldTable->rowCount(); ++r) {
        QMap<QString, QString> field;
        auto *nameItem = m_classFieldTable->item(r, 0);
        auto *typeItem = m_classFieldTable->item(r, 1);
        auto *visCombo = qobject_cast<QComboBox*>(m_classFieldTable->cellWidget(r, 2));
        auto *staticCheck = qobject_cast<QCheckBox*>(m_classFieldTable->cellWidget(r, 3));
        field["name"] = nameItem ? nameItem->text() : QString();
        field["type"] = typeItem ? typeItem->text() : QString();
        field["visibility"] = visCombo ? visCombo->currentText() : "+";
        field["static"] = (staticCheck && staticCheck->isChecked()) ? "true" : "false";
        data.fields.append(field);
    }
    for (int r = 0; r < m_classMethodTable->rowCount(); ++r) {
        QMap<QString, QString> method;
        auto *nameItem = m_classMethodTable->item(r, 0);
        auto *retItem = m_classMethodTable->item(r, 1);
        auto *paramsItem = m_classMethodTable->item(r, 2);
        auto *visCombo = qobject_cast<QComboBox*>(m_classMethodTable->cellWidget(r, 3));
        method["name"] = nameItem ? nameItem->text() : QString();
        method["returnType"] = retItem ? retItem->text() : QString();
        method["parameters"] = paramsItem ? paramsItem->text() : QString();
        method["visibility"] = visCombo ? visCombo->currentText() : "+";
        data.methods.append(method);
    }
    m_classData[m_lastClassRow] = data;
}
void MermaidDialog::loadClassData(int classRow)
{
    m_classFieldTable->blockSignals(true);
    m_classMethodTable->blockSignals(true);
    m_classFieldTable->setRowCount(0);
    m_classMethodTable->setRowCount(0);
    if (classRow < 0) {
        m_classFieldTable->blockSignals(false);
        m_classMethodTable->blockSignals(false);
        return;
    }
    ClassData data = m_classData.value(classRow);
    for (const auto &field : data.fields) {
        int r = m_classFieldTable->rowCount();
        m_classFieldTable->insertRow(r);
        m_classFieldTable->setItem(r, 0, new QTableWidgetItem(field.value("name")));
        m_classFieldTable->setItem(r, 1, new QTableWidgetItem(field.value("type")));
        auto *visCombo = new QComboBox; visCombo->addItems({"+", "-", "#", "~"});
        int idx = visCombo->findText(field.value("visibility", "+"));
        if (idx >= 0) visCombo->setCurrentIndex(idx);
        m_classFieldTable->setCellWidget(r, 2, visCombo);
        auto *staticCheck = new QCheckBox;
        staticCheck->setCheckState(field.value("static") == "true" ? Qt::Checked : Qt::Unchecked);
        m_classFieldTable->setCellWidget(r, 3, staticCheck);
    }
    for (const auto &method : data.methods) {
        int r = m_classMethodTable->rowCount();
        m_classMethodTable->insertRow(r);
        m_classMethodTable->setItem(r, 0, new QTableWidgetItem(method.value("name")));
        m_classMethodTable->setItem(r, 1, new QTableWidgetItem(method.value("returnType")));
        m_classMethodTable->setItem(r, 2, new QTableWidgetItem(method.value("parameters")));
        auto *visCombo = new QComboBox; visCombo->addItems({"+", "-", "#", "~"});
        int idx = visCombo->findText(method.value("visibility", "+"));
        if (idx >= 0) visCombo->setCurrentIndex(idx);
        m_classMethodTable->setCellWidget(r, 3, visCombo);
    }
    m_classFieldTable->blockSignals(false);
    m_classMethodTable->blockSignals(false);
}
void MermaidDialog::refreshClassRelCombos()
{
    QStringList names;
    for (int r = 0; r < m_classTable->rowCount(); ++r) {
        auto *item = m_classTable->item(r, 0);
        if (item && !item->text().isEmpty()) names.append(item->text());
    }
    for (int r = 0; r < m_classRelationTable->rowCount(); ++r) {
        for (int col : {0, 1}) {
            auto *combo = qobject_cast<QComboBox*>(m_classRelationTable->cellWidget(r, col));
            if (!combo) continue;
            QString prev = combo->currentText();
            combo->blockSignals(true);
            combo->clear();
            combo->addItems(names);
            int idx = combo->findText(prev);
            if (idx >= 0) combo->setCurrentIndex(idx);
            combo->blockSignals(false);
        }
    }
}
QString MermaidDialog::buildClassDiagram() const
{
    QString out = "classDiagram\n";
    for (int r = 0; r < m_classRelationTable->rowCount(); ++r) {
        auto *fromCombo = qobject_cast<QComboBox*>(m_classRelationTable->cellWidget(r, 0));
        auto *toCombo = qobject_cast<QComboBox*>(m_classRelationTable->cellWidget(r, 1));
        auto *typeCombo = qobject_cast<QComboBox*>(m_classRelationTable->cellWidget(r, 2));
        auto *labelItem = m_classRelationTable->item(r, 3);
        if (!fromCombo || !toCombo || !typeCombo) continue;
        QString from = fromCombo->currentText();
        QString to = toCombo->currentText();
        QString type = typeCombo->currentText();
        QString label = labelItem ? labelItem->text() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + " " + type + " " + to;
        if (!label.isEmpty()) out += " : " + label;
        out += "\n";
    }
    for (int c = 0; c < m_classTable->rowCount(); ++c) {
        auto *nameItem = m_classTable->item(c, 0);
        auto *typeCombo = qobject_cast<QComboBox*>(m_classTable->cellWidget(c, 1));
        if (!nameItem || nameItem->text().isEmpty()) continue;
        QString className = nameItem->text();
        QString classType = typeCombo ? typeCombo->currentText() : "class";
        ClassData data = m_classData.value(c);
        if (classType == "Enumeration")
            out += "    enum " + className + " {\n";
        else
            out += "    class " + className + " {\n";
        for (const auto &field : data.fields) {
            QString vis = field.value("visibility", "+");
            out += "        " + vis + field.value("type") + " " + field.value("name");
            if (field.value("static") == "true") out += " $";
            out += "\n";
        }
        for (const auto &method : data.methods) {
            QString vis = method.value("visibility", "+");
            out += "        " + vis + method.value("name") + "(" + method.value("parameters") + ") " + method.value("returnType") + "\n";
        }
        out += "    }\n";
    }
    return out;
}
void MermaidDialog::saveCurrentERAttrs()
{
    if (m_lastEntityRow < 0) return;
    QList<QMap<QString, QString>> attrs;
    for (int r = 0; r < m_erAttributeTable->rowCount(); ++r) {
        QMap<QString, QString> attr;
        auto *nameItem = m_erAttributeTable->item(r, 0);
        auto *typeItem = m_erAttributeTable->item(r, 1);
        auto *keyCombo = qobject_cast<QComboBox*>(m_erAttributeTable->cellWidget(r, 2));
        attr["name"] = nameItem ? nameItem->text() : QString();
        attr["type"] = typeItem ? typeItem->text() : QString();
        attr["key"] = keyCombo ? keyCombo->currentText() : QString();
        attrs.append(attr);
    }
    m_erEntityAttrs[m_lastEntityRow] = attrs;
}
void MermaidDialog::loadERAttrs(int entityRow)
{
    m_erAttributeTable->blockSignals(true);
    m_erAttributeTable->setRowCount(0);
    if (entityRow < 0) {
        m_erAttributeTable->blockSignals(false);
        return;
    }
    auto attrs = m_erEntityAttrs.value(entityRow);
    for (const auto &attr : attrs) {
        int r = m_erAttributeTable->rowCount();
        m_erAttributeTable->insertRow(r);
        m_erAttributeTable->setItem(r, 0, new QTableWidgetItem(attr.value("name")));
        m_erAttributeTable->setItem(r, 1, new QTableWidgetItem(attr.value("type")));
        auto *keyCombo = new QComboBox; keyCombo->addItems({"", "PK", "FK"});
        int idx = keyCombo->findText(attr.value("key"));
        if (idx >= 0) keyCombo->setCurrentIndex(idx);
        m_erAttributeTable->setCellWidget(r, 2, keyCombo);
    }
    m_erAttributeTable->blockSignals(false);
}
void MermaidDialog::refreshERRelCombos()
{
    QStringList names;
    for (int r = 0; r < m_erEntityTable->rowCount(); ++r) {
        auto *item = m_erEntityTable->item(r, 0);
        if (item && !item->text().isEmpty()) names.append(item->text());
    }
    for (int r = 0; r < m_erRelationTable->rowCount(); ++r) {
        for (int col : {0, 1}) {
            auto *combo = qobject_cast<QComboBox*>(m_erRelationTable->cellWidget(r, col));
            if (!combo) continue;
            QString prev = combo->currentText();
            combo->blockSignals(true);
            combo->clear();
            combo->addItems(names);
            int idx = combo->findText(prev);
            if (idx >= 0) combo->setCurrentIndex(idx);
            combo->blockSignals(false);
        }
    }
}
QString MermaidDialog::buildERDiagram() const
{
    QString out = "erDiagram\n";
    for (int r = 0; r < m_erRelationTable->rowCount(); ++r) {
        auto *fromCombo = qobject_cast<QComboBox*>(m_erRelationTable->cellWidget(r, 0));
        auto *toCombo = qobject_cast<QComboBox*>(m_erRelationTable->cellWidget(r, 1));
        auto *typeCombo = qobject_cast<QComboBox*>(m_erRelationTable->cellWidget(r, 2));
        auto *labelItem = m_erRelationTable->item(r, 3);
        if (!fromCombo || !toCombo || !typeCombo) continue;
        QString from = fromCombo->currentText();
        QString to = toCombo->currentText();
        QString type = typeCombo->currentText();
        QString label = labelItem ? labelItem->text() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + " " + type + " " + to;
        if (!label.isEmpty()) out += " : " + label;
        out += "\n";
    }
    for (int e = 0; e < m_erEntityTable->rowCount(); ++e) {
        auto *nameItem = m_erEntityTable->item(e, 0);
        if (!nameItem || nameItem->text().isEmpty()) continue;
        QString entityName = nameItem->text();
        auto attrs = m_erEntityAttrs.value(e);
        if (attrs.isEmpty()) continue;
        out += "    " + entityName + " {\n";
        for (const auto &attr : attrs)
            out += "        " + attr.value("type") + " " + attr.value("name")
                   + (attr.value("key").isEmpty() ? "" : " " + attr.value("key")) + "\n";
        out += "    }\n";
    }
    return out;
}
