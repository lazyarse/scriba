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

#include <QComboBox>
#include <QTableWidget>

void MermaidDialog::refreshStateCombos()
{
    QStringList stateNames;
    for (int r = 0; r < m_stateTable->rowCount(); ++r) {
        auto *item = m_stateTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            stateNames.append(item->text().trimmed());
    }
    populateComboColumns(m_stateTransitionTable, {0, 1}, stateNames);
}
QString MermaidDialog::buildStateDiagram() const
{
    QString out = "stateDiagram-v2\n";
    // Rows with a Section (composite state) are emitted inside `state X { ... }`
    // blocks; the block opens when the section first appears and closes when the
    // section changes (or at the end), mirroring the parsed source structure.
    int openSection = -1; // index into sections, -1 when no block is open
    QList<QString> sections;
    auto closeSection = [&out, &openSection]() {
        if (openSection >= 0) {
            out += "    }\n";
            openSection = -1;
        }
    };
    for (int r = 0; r < m_stateTransitionTable->rowCount(); ++r) {
        auto *fromBox = qobject_cast<QComboBox*>(m_stateTransitionTable->cellWidget(r, 0));
        auto *toBox = qobject_cast<QComboBox*>(m_stateTransitionTable->cellWidget(r, 1));
        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        auto *labelItem = m_stateTransitionTable->item(r, 2);
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        auto *sectionItem = m_stateTransitionTable->item(r, 3);
        QString section = sectionItem ? sectionItem->text().trimmed() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;

        int sectionIdx = -1;
        if (!section.isEmpty()) {
            sectionIdx = sections.indexOf(section);
            if (sectionIdx < 0) {
                sections.append(section);
                sectionIdx = sections.size() - 1;
            }
        }
        if (sectionIdx != openSection) {
            closeSection();
            if (sectionIdx >= 0)
                out += "    state " + section + " {\n";
            openSection = sectionIdx;
        }
        out += sectionIdx >= 0 ? "        " : "    ";
        out += from + " --> " + to;
        if (!label.isEmpty()) out += " : " + label;
        out += "\n";
    }
    closeSection();
    return out;
}
