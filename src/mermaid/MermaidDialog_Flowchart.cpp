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
#include <QStringList>
#include <QTableWidget>

// Arrow glyph table for edge rows; shared by the flowchart panel (defined in
// MermaidDialog.cpp) and the flowchart/sequence builders here.
struct ArrowInfo {
    QString display;
    QString left;
    QString right;
};

static const ArrowInfo kArrowTypes[] = {
    {"-->",  "--",  "-->"},
    {"---",  "--",  "---"},
    {"-.->", "-.",  ".->"},
    {"==>",  "==",  "==>"},
    {"--o",  "--",  "--o"},
    {"--x",  "--",  "--x"},
    {"<-->", "<--", "-->"},
    {"<==>", "<==", "==>"},
    {"<-.->", "<-.", ".->"},
    {"o--o", "o--", "--o"},
    {"x--x", "x--", "--x"},
};

static const int kArrowCount = sizeof(kArrowTypes) / sizeof(ArrowInfo);

void MermaidDialog::refreshEdgeNodeCombos()
{
    QStringList nodeIds;
    for (int r = 0; r < m_fcNodeTable->rowCount(); ++r) {
        auto *item = m_fcNodeTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            nodeIds.append(item->text().trimmed());
    }
    populateComboColumns(m_fcEdgeTable, {0, 1}, nodeIds);
    for (int r = 0; r < m_fcEdgeTable->rowCount(); ++r) {
        auto *arrowBox = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 3));
        if (!arrowBox) {
            arrowBox = new QComboBox(m_fcEdgeTable);
            for (int i = 0; i < kArrowCount; ++i)
                arrowBox->addItem(kArrowTypes[i].display);
            m_fcEdgeTable->setCellWidget(r, 3, arrowBox);
            connect(arrowBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialog::schedulePreviewUpdate);
        }
    }
}
static QString shapeToMermaid(const QString &shape, const QString &text)
{
    if (shape == "Round")
        return "(" + text + ")";
    if (shape == "Stadium")
        return "([" + text + "])";
    if (shape == "Diamond")
        return "{" + text + "}";
    if (shape == "Hexagon")
        return "{{" + text + "}}";
    return "[" + text + "]";
}
static QString renderEdge(const QString &from, const QString &to,
                          const QString &label, const QString &arrowType)
{
    const ArrowInfo *info = nullptr;
    for (int i = 0; i < kArrowCount; ++i) {
        if (kArrowTypes[i].display == arrowType) {
            info = &kArrowTypes[i];
            break;
        }
    }
    if (!info) return from + "-->" + to;
    if (label.isEmpty()) return from + info->display + to;
    return from + info->left + " " + label + " " + info->right + to;
}
QString MermaidDialog::buildFlowchartDiagram() const
{
    QString dir = m_fcDirection->currentData().toString();
    QString out = "flowchart " + dir + "\n";

    for (int r = 0; r < m_fcNodeTable->rowCount(); ++r) {
        auto *idItem = m_fcNodeTable->item(r, 0);
        auto *textItem = m_fcNodeTable->item(r, 1);
        auto *shapeBox = qobject_cast<QComboBox*>(m_fcNodeTable->cellWidget(r, 2));
        QString id = idItem ? idItem->text().trimmed() : QString();
        QString text = textItem ? textItem->text().trimmed() : id;
        QString shape = shapeBox ? shapeBox->currentText() : "box";
        if (id.isEmpty()) continue;
        if (text.isEmpty()) text = id;
        out += "    " + id + shapeToMermaid(shape, text) + "\n";
    }

    for (int r = 0; r < m_fcEdgeTable->rowCount(); ++r) {
        auto *fromBox = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 0));
        auto *toBox = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 1));
        auto *labelItem = m_fcEdgeTable->item(r, 2);
        auto *arrowBox = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 3));
        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString arrow = arrowBox ? arrowBox->currentText() : QString("-->");
        if (from.isEmpty() || to.isEmpty() || from == to) continue;
        out += "    " + renderEdge(from, to, label, arrow) + "\n";
    }
    return out;
}
void MermaidDialog::refreshMessageCombos()
{
    QStringList names;
    for (int r = 0; r < m_seqParticipantTable->rowCount(); ++r) {
        auto *item = m_seqParticipantTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            names.append(item->text().trimmed());
    }
    populateComboColumns(m_seqMessageTable, {0, 1}, names);
    for (int r = 0; r < m_seqMessageTable->rowCount(); ++r) {
        auto *arrowBox = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 3));
        if (!arrowBox) {
            arrowBox = new QComboBox(m_seqMessageTable);
            arrowBox->addItems({"->>", "-->>", "-x", "--)", "->", "-->"});
            m_seqMessageTable->setCellWidget(r, 3, arrowBox);
            connect(arrowBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialog::schedulePreviewUpdate);
        }
    }
}
QString MermaidDialog::buildSequenceDiagram() const
{
    QString out = "sequenceDiagram\n";

    for (int r = 0; r < m_seqParticipantTable->rowCount(); ++r) {
        auto *nameItem = m_seqParticipantTable->item(r, 0);
        auto *aliasItem = m_seqParticipantTable->item(r, 1);
        QString name = nameItem ? nameItem->text().trimmed() : QString();
        QString alias = aliasItem ? aliasItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;
        out += "    participant " + name;
        if (!alias.isEmpty())
            out += " as " + alias;
        out += "\n";
    }

    for (int r = 0; r < m_seqMessageTable->rowCount(); ++r) {
        auto *fromBox = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 0));
        auto *toBox = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 1));
        auto *labelItem = m_seqMessageTable->item(r, 2);
        auto *arrowBox = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 3));
        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString arrow = arrowBox ? arrowBox->currentText() : "->>";
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + arrow + to + ": " + label + "\n";
    }
    return out;
}
