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
#pragma once

// Reverse-parse Mermaid diagrams back into the fields of the Mermaid dialog,
// so an existing rendered diagram can be re-opened with its settings
// pre-filled. Pure logic — no Qt widgets — so it is unit-testable without
// WebEngine. Targets the exact output format of the Mermaid dialog.

#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

namespace ChartSource {

// ---------------------------------------------------------------------------
// Mermaid (` ```mermaid ` blocks)
// ---------------------------------------------------------------------------

enum class MermaidType {
    Pie, Flowchart, Sequence, Gantt, Class, ER, State, Mindmap,
    Timeline, Journey, Quadrant, Sankey, Unknown
};

MermaidType detectMermaidType(const QString &diagram);

// Reconstruction of a Mermaid diagram into the fields of the Mermaid dialog.
// Only the members belonging to `type` are populated. When `parseable` is
// false the caller should fall back to editing `source` raw.
struct MermaidData {
    QString source;            // the original diagram text
    MermaidType type = MermaidType::Unknown;
    bool parseable = false;

    // Pie
    QString pieTitle;
    QList<QPair<QString, double>> pieEntries;

    // Flowchart
    QString fcDirection;
    QList<QStringList> fcNodes; // id, text, shape (Box|Round|Stadium|Diamond|Hexagon)
    QList<QStringList> fcEdges; // from, to, label, arrow (display, e.g. "-->")

    // Sequence
    QList<QStringList> seqParticipants; // name, alias
    QList<QStringList> seqMessages;     // from, to, label, arrow

    // Gantt
    QString ganttTitle;
    QString ganttDateFormat;
    bool ganttWeekend = true;
    QList<QStringList> ganttTasks; // id, description, start, duration, status, section

    // State
    QList<QStringList> stateTransitions; // from, to, label, section (composite state)

    // Mindmap
    struct TreeNode {
        QString text;
        QList<TreeNode> children;
    };
    QList<TreeNode> mindmapRoots; // typically one root

    // Timeline / Journey (section repeated per entry, matching the dialog table)
    QString timelineTitle;
    QList<QStringList> timelineEntries; // section, event
    QString journeyTitle;
    QList<QStringList> journeyEntries;  // section, task, score, actors

    // Quadrant
    QString quadTitle;
    QString quadXLeft, quadXRight, quadYBottom, quadYTop;
    QString quadQ1, quadQ2, quadQ3, quadQ4;
    QList<QStringList> quadPoints; // label, x, y

    // Sankey
    QList<QStringList> sankeyLinks; // source, target, value
};

bool parseMermaid(const QString &diagram, MermaidData &out);

} // namespace ChartSource
