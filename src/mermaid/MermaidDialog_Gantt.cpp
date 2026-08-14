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
#include <QLineEdit>
#include <QMap>
#include <QTableWidget>

QString MermaidDialog::buildGanttDiagram() const
{
    QString out = "gantt\n";
    QString title = m_ganttTitle->text().trimmed();
    if (!title.isEmpty())
        out += "    title " + title + "\n";
    out += "    dateFormat " + m_ganttDateFormat->currentText() + "\n";
    if (m_ganttWeekend->isChecked())
        out += "    excludes weekends\n";

    QMap<QString, QList<int>> sections;
    for (int r = 0; r < m_ganttTaskTable->rowCount(); ++r) {
        auto *sectionItem = m_ganttTaskTable->item(r, 5);
        QString section = sectionItem ? sectionItem->text().trimmed() : QString();
        if (section.isEmpty()) section = "(none)";
        sections[section].append(r);
    }

    for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
        out += "    section " + it.key() + "\n";
        for (int r : it.value()) {
            auto *idItem = m_ganttTaskTable->item(r, 0);
            auto *descItem = m_ganttTaskTable->item(r, 1);
            auto *startItem = m_ganttTaskTable->item(r, 2);
            auto *durationItem = m_ganttTaskTable->item(r, 3);
            auto *statusBox = qobject_cast<QComboBox*>(m_ganttTaskTable->cellWidget(r, 4));
            QString id = idItem ? idItem->text().trimmed() : QString();
            QString desc = descItem ? descItem->text().trimmed() : QString();
            QString start = startItem ? startItem->text().trimmed() : QString();
            QString duration = durationItem ? durationItem->text().trimmed() : QString();
            QString status = statusBox ? statusBox->currentData().toString() : QString();
            if (id.isEmpty()) continue;
            QString taskLine = "        " + desc;
            if (!status.isEmpty())
                taskLine += " :" + status + ", ";
            else
                taskLine += " : ";
            taskLine += id + ", " + start + ", " + duration;
            out += taskLine + "\n";
        }
    }
    return out;
}
