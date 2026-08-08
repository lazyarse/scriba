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
#include "MermaidParser.h"

#include <QRegularExpression>

namespace ChartSource {

// Strips a leading `--- ... ---` YAML frontmatter block (as used for Mermaid
// config directives) so the diagram keyword that follows it is detected and
// parsed. Returns "true" when a frontmatter block was removed.
static bool stripFrontmatter(QString &diagram)
{
    const QStringList lines = diagram.split('\n');
    int start = -1;
    int end = lines.size();
    bool sawConfig = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed().isEmpty())
            continue;
        if (lines.at(i).trimmed() == QLatin1String("---")) {
            if (start < 0) {
                start = i;
                continue;
            }
            end = i;
            break;
        }
        if (start < 0)
            break;
        sawConfig = true;
    }
    if (start < 0 || end >= lines.size() || !sawConfig)
        return false;
    diagram = lines.mid(end + 1).join('\n');
    return true;
}

// ---------------------------------------------------------------------------
// Mermaid — detection
// ---------------------------------------------------------------------------

MermaidType detectMermaidType(const QString &rawDiagram)
{
    QString diagram = rawDiagram;
    stripFrontmatter(diagram);
    for (const QString &rawLine : diagram.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        // `%% comment` lines and `%%{init: {...}}%%` integrity/config
        // directives are commonplace at the top of a diagram; skip them while
        // hunting for the diagram keyword.
        if (line.startsWith(QLatin1String("%%"))
            || line.startsWith(QLatin1String("---")))
            continue;
        if (line.startsWith(QLatin1String("pie"))) return MermaidType::Pie;
        if (line.startsWith(QLatin1String("flowchart")) || line.startsWith(QLatin1String("graph")))
            return MermaidType::Flowchart;
        if (line.startsWith(QLatin1String("sequenceDiagram"))) return MermaidType::Sequence;
        if (line.startsWith(QLatin1String("gantt"))) return MermaidType::Gantt;
        if (line.startsWith(QLatin1String("classDiagram"))) return MermaidType::Class;
        if (line.startsWith(QLatin1String("erDiagram"))) return MermaidType::ER;
        if (line.startsWith(QLatin1String("stateDiagram"))) return MermaidType::State;
        if (line.startsWith(QLatin1String("mindmap"))) return MermaidType::Mindmap;
        if (line.startsWith(QLatin1String("timeline"))) return MermaidType::Timeline;
        if (line.startsWith(QLatin1String("journey"))) return MermaidType::Journey;
        if (line.startsWith(QLatin1String("quadrantChart"))) return MermaidType::Quadrant;
        if (line.startsWith(QLatin1String("sankey"))) return MermaidType::Sankey;
        return MermaidType::Unknown;
    }
    return MermaidType::Unknown;
}

static QStringList splitCsv(const QString &line)
{
    QStringList out;
    QString cur;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == ',') {
            out.append(cur.trimmed());
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.append(cur.trimmed());
    return out;
}

// ---------------------------------------------------------------------------
// Mermaid — Pie
// ---------------------------------------------------------------------------

static bool parsePie(const QString &diagram, MermaidData &out)
{
    bool parsed = false;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("pie"))) {
            const QString rest = line.mid(3).trimmed();
            if (rest.startsWith(QLatin1String("title"))) {
                out.pieTitle = rest.mid(5).trimmed();
                parsed = true;
            }
            continue;
        }
        // `title X` may legitimately sit on its own line after the `pie` header
        // (mermaid accepts both `pie title X` and `pie\n title X`).
        if (line.startsWith(QLatin1String("title "))) {
            out.pieTitle = line.mid(6).trimmed();
            parsed = true;
            continue;
        }
        // `"label" : value`
        const QRegularExpression re(
            QStringLiteral("^\"?(.+?)\"?\\s*:\\s*([\\d.eE+-]+)$"));
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;
        bool ok = false;
        const double value = m.captured(2).toDouble(&ok);
        if (!ok)
            continue;
        out.pieEntries.append({m.captured(1).trimmed(), value});
        parsed = true;
    }
    out.parseable = parsed && !out.pieEntries.isEmpty();
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Flowchart
// ---------------------------------------------------------------------------

struct ArrowDef {
    const char *display;
    const char *left;
    const char *right;
};

static const ArrowDef kArrows[] = {
    {"-->",  "--",  "-->"},
    {"---",  "--",  "---"},
    {"-.->", "-.",  ".->"},
    {"==>",  "==",  "==>"},
    {"--o",  "--",  "--o"},
    {"--x",  "--",  "--x"},
    // Bidirectional (both ends marked): `A <--> B`, `A <==> B`,
    // `A <-.-> B`, `A o--o B`, `A x--x B`.
    {"<-->", "<--", "-->"},
    {"<==>", "<==", "==>"},
    {"<-.->", "<-.", ".->"},
    {"o--o", "o--", "--o"},
    {"x--x", "x--", "--x"},
};

static QString arrowDisplayForTokens(const QString &left, const QString &right)
{
    for (const ArrowDef &a : kArrows)
        if (left == QLatin1String(a.left) && right == QLatin1String(a.right))
            return QLatin1String(a.display);
    return {};
}

struct FlowNodeTok {
    QString id;
    QString text;
    QString shape;
    bool shaped = false; // declared with an explicit shape (vs bare id)
    bool ok = false;
};

// Parses a node token: `A`, `A[text]`, `A(text)`, `A([text])`, `A{text}`,
// `A{{text}}`. Bare ids must be single tokens (no whitespace) and fall back
// to a plain box with the id as text (mermaid auto-creates such nodes).
static FlowNodeTok parseFlowNodeToken(const QString &tok)
{
    FlowNodeTok r;
    const QString t = tok.trimmed();
    if (t.isEmpty())
        return r;
    int shapeStart = -1;
    for (int i = 0; i < t.size(); ++i) {
        const QChar c = t.at(i);
        if (c == '[' || c == '(' || c == '{') { shapeStart = i; break; }
    }
    if (shapeStart < 0) {
        if (t.contains(QLatin1Char(' ')))
            return r;
        r.id = t;
        r.text = t;
        r.shape = QStringLiteral("Box");
        r.shaped = false;
        r.ok = true;
        return r;
    }
    r.id = t.left(shapeStart).trimmed();
    if (r.id.isEmpty())
        return r;
    const QString rest = t.mid(shapeStart);
    if (rest.startsWith(QLatin1String("{{")) && rest.endsWith(QLatin1String("}}"))) {
        r.text = rest.mid(2).chopped(2); r.shape = QStringLiteral("Hexagon");
    } else if (rest.startsWith(QLatin1String("([")) && rest.endsWith(QLatin1String("])"))) {
        r.text = rest.mid(2).chopped(2); r.shape = QStringLiteral("Stadium");
    } else if (rest.startsWith(QLatin1String("[")) && rest.endsWith(QLatin1String("]"))) {
        r.text = rest.mid(1).chopped(1); r.shape = QStringLiteral("Box");
    } else if (rest.startsWith(QLatin1String("(")) && rest.endsWith(QLatin1String(")"))) {
        r.text = rest.mid(1).chopped(1); r.shape = QStringLiteral("Round");
    } else if (rest.startsWith(QLatin1String("{")) && rest.endsWith(QLatin1String("}"))) {
        r.text = rest.mid(1).chopped(1); r.shape = QStringLiteral("Diamond");
    } else {
        return r;
    }
    r.shaped = true;
    r.ok = true;
    return r;
}

// Adds a node, deduplicating by id. A shaped declaration supersedes an
// earlier bare (auto-created) reference; bare references never clobber an
// existing shaped node.
static void flowEnsureNode(MermaidData &out, const FlowNodeTok &n)
{
    for (auto &existing : out.fcNodes) {
        if (existing[0] == n.id) {
            if (n.shaped) {
                existing[1] = n.text;
                existing[2] = n.shape;
            }
            return;
        }
    }
    out.fcNodes.append({n.id, n.text, n.shape});
}

static bool parseFlowchartLine(const QString &line, MermaidData &out)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return false;

    // Spaced-label edge: `A -- label --> B`, `A == label ==> B`,
    // `A .- label .-> B`, `A <-- label --> B`, `A o-- label --o B`, ...
    static const QRegularExpression labelRe(
        QStringLiteral(R"(^(.+?)\s+(--|==|\.-|o--|x--|<--|<==|<-.)\s+(.+?)\s+(-->|---|==>|\.->|--o|--x)\s+(.+)$)"));
    QRegularExpressionMatch em = labelRe.match(trimmed);
    if (em.hasMatch()) {
        const QString display = arrowDisplayForTokens(em.captured(2), em.captured(4));
        if (!display.isEmpty()) {
            const FlowNodeTok from = parseFlowNodeToken(em.captured(1));
            const FlowNodeTok to = parseFlowNodeToken(em.captured(5));
            if (from.ok && to.ok && from.id != to.id) {
                flowEnsureNode(out, from);
                flowEnsureNode(out, to);
                out.fcEdges.append({from.id, to.id, em.captured(3).trimmed(), display});
                return true;
            }
        }
    }

    // Bare / inline-pipe edge: `A-->B`, `A --> B`, `A -->|label| B`,
    // `A[text] --> B{text}` (a node and edge on one line).
    static const QRegularExpression edgeRe(
        QStringLiteral(R"(^(.+?)\s*(-->|---|-\.->|==>|--o|--x|<-->|<==>|<-.->|o--o|x--x)(?:\s*\|\s*([^|]*)\s*\|)?\s*(.+)$)"));
    em = edgeRe.match(trimmed);
    if (em.hasMatch()) {
        const FlowNodeTok from = parseFlowNodeToken(em.captured(1));
        const FlowNodeTok to = parseFlowNodeToken(em.captured(4));
        if (from.ok && to.ok && from.id != to.id) {
            flowEnsureNode(out, from);
            flowEnsureNode(out, to);
            out.fcEdges.append({from.id, to.id, em.captured(3).trimmed(), em.captured(2)});
            return true;
        }
    }

    // Standalone node: `A[text]`, `A(text)`, `A{text}`, ...
    const FlowNodeTok node = parseFlowNodeToken(trimmed);
    if (node.ok && node.shaped) {
        flowEnsureNode(out, node);
        return true;
    }
    return false;
}

static bool parseFlowchart(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("flowchart")) || line.startsWith(QLatin1String("graph"))) {
            const QString dir = line.section(' ', 1).trimmed();
            if (dir.size() == 2) {
                out.fcDirection = dir;
                sawHeader = true;
            }
            continue;
        }
        if (parseFlowchartLine(line, out))
            sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Sequence
// ---------------------------------------------------------------------------

static bool parseSequence(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    // Mermaid auto-creates an actor for any name that appears in a message, so
    // the parser mirrors that: message endpoints become participants too, in
    // order of first appearance. Explicit `participant` lines stay authoritative
    // for ordering and aliases.
    auto ensureParticipant = [&out](const QString &name, const QString &alias = QString()) {
        if (name.isEmpty())
            return;
        for (const auto &p : out.seqParticipants) {
            if (p[0] == name || (!p[1].isEmpty() && p[1] == name))
                return; // already declared, or the name is an existing alias
        }
        out.seqParticipants.append({name, alias});
    };
    // Maps a message endpoint to the participant's canonical name, following
    // aliases (`A` -> `Alice` for `participant Alice as A`).
    auto canonicalName = [&out](const QString &name) -> QString {
        for (const auto &p : out.seqParticipants) {
            if (p[0] == name)
                return name;
            if (!p[1].isEmpty() && p[1] == name)
                return p[0];
        }
        return name;
    };
    // Renames every message referencing the old id to the participant name.
    auto renameInMessages = [&out](const QString &oldName, const QString &newName) {
        for (auto &msg : out.seqMessages) {
            if (msg[0] == oldName) msg[0] = newName;
            if (msg[1] == oldName) msg[1] = newName;
        }
    };
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("sequenceDiagram"))) {
            sawHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1String("participant "))) {
            const QString rest = line.mid(12).trimmed();
            const int asIdx = rest.indexOf(QLatin1String(" as "));
            const QString name = asIdx > 0 ? rest.left(asIdx).trimmed() : rest.trimmed();
            const QString alias = asIdx > 0 ? rest.mid(asIdx + 4).trimmed() : QString();
            // If the actor was implicitly added from a message (by name or by
            // the alias messages referenced before the declaration), upgrade it
            // with the declared name/alias instead of duplicating it.
            bool found = false;
            for (auto &p : out.seqParticipants) {
                if (p[0] == name) {
                    if (!alias.isEmpty())
                        p[1] = alias;
                    found = true;
                    break;
                }
            }
            if (!found) {
                const QString old = alias.isEmpty() ? name : alias;
                for (int i = 0; i < out.seqParticipants.size(); ++i) {
                    if (out.seqParticipants[i][0] == old) {
                        out.seqParticipants[i][0] = name;
                        out.seqParticipants[i][1] = alias;
                        renameInMessages(old, name);
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                ensureParticipant(name, alias);
            sawContent = true;
            continue;
        }
        // `A->>B: msg`, `A -->> B : msg`, `A--x B: msg`
        static const QRegularExpression re(
            QStringLiteral(R"(^(\S+?)\s*(->>|-->>|-->|--\)|-x|->)\s*(\S+?)\s*:\s*(.*)$)"));
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;
        const QString from = m.captured(1);
        const QString to = m.captured(3);
        if (from.isEmpty() || to.isEmpty())
            continue;
        ensureParticipant(from);
        ensureParticipant(to);
        out.seqMessages.append({canonicalName(from), canonicalName(to),
                                m.captured(4).trimmed(), m.captured(2)});
        sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Gantt
// ---------------------------------------------------------------------------

static bool parseGantt(const QString &diagram, MermaidData &out)
{
    out.ganttWeekend = false;
    out.ganttDateFormat = QStringLiteral("YYYY-MM-DD");
    bool sawHeader = false;
    bool sawContent = false;
    QString section;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("gantt"))) {
            sawHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1String("title "))) {
            out.ganttTitle = line.mid(6).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1String("dateFormat "))) {
            out.ganttDateFormat = line.mid(11).trimmed();
            continue;
        }
        if (line == QLatin1String("excludes weekends")) {
            out.ganttWeekend = true;
            continue;
        }
        if (line.startsWith(QLatin1String("section "))) {
            section = line.mid(8).trimmed();
            continue;
        }
        // `desc :status, id, start, duration` (status may be omitted)
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString desc = line.left(colon).trimmed();
        const QString rest = line.mid(colon + 1).trimmed();
        const QStringList parts = splitCsv(rest);
        if (parts.size() < 3)
            continue;
        QString status = parts[0].trimmed();
        QString id, start, duration;
        const QSet<QString> statuses = {QStringLiteral("done"), QStringLiteral("active"),
                                        QStringLiteral("crit"), QStringLiteral("milestone")};
        if (statuses.contains(status)) {
            if (parts.size() < 4)
                continue;
            id = parts[1].trimmed();
            start = parts[2].trimmed();
            duration = parts[3].trimmed();
        } else {
            status.clear();
            id = parts[0].trimmed();
            start = parts[1].trimmed();
            duration = parts[2].trimmed();
        }
        if (id.isEmpty())
            continue;
        out.ganttTasks.append({id, desc, start, duration, status, section});
        sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — State
// ---------------------------------------------------------------------------

static bool parseState(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    // Composite state blocks (`state X { ... }`) group their inner transitions
    // into a section named after the composite state, mirroring how mermaid
    // renders a sub-state diagram inside the composite box.
    QStringList sectionStack;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("stateDiagram"))) {
            sawHeader = true;
            continue;
        }
        // `state X {` opens a composite block. Handle both the bare id form
        // and the `state "description" as X` alias form.
        static const QRegularExpression openRe(
            QStringLiteral(R"(^state\s+(.+?)\s*\{$)"));
        const QRegularExpressionMatch om = openRe.match(line);
        if (om.hasMatch()) {
            QString id = om.captured(1).trimmed();
            const int asIdx = id.indexOf(QLatin1String(" as "));
            if (asIdx > 0)
                id = id.mid(asIdx + 4).trimmed();
            sectionStack.append(id);
            continue;
        }
        if (line == QLatin1String("}")) {
            if (!sectionStack.isEmpty())
                sectionStack.removeLast();
            continue;
        }
        // `A --> B` or `A --> B : label`
        static const QRegularExpression re(
            QStringLiteral(R"(^([^\s:]+)\s+-->\s+([^\s:]+)(?:\s*:\s*(.*))?$)"));
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;
        if (m.captured(1).isEmpty() || m.captured(2).isEmpty())
            continue;
        const QString section = sectionStack.isEmpty() ? QString() : sectionStack.last();
        out.stateTransitions.append({m.captured(1), m.captured(2), m.captured(3).trimmed(),
                                     section});
        sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Mindmap
// ---------------------------------------------------------------------------

static bool parseMindmap(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    QList<QPair<int, MermaidData::TreeNode *>> stack; // (indent, node)
    for (const QString &rawLine : diagram.split('\n')) {
        if (rawLine.trimmed().isEmpty())
            continue;
        const QString trimmed = rawLine.trimmed();
        if (trimmed.startsWith(QLatin1String("mindmap"))) {
            sawHeader = true;
            continue;
        }
        // Indentation column: each tab counts as four columns, so both
        // tab- and space-indented trees (any step size) nest correctly
        // without assuming a fixed 4-space indent like depth = spaces/4.
        int indent = 0;
        for (QChar c : rawLine) {
            if (c == QLatin1Char(' '))
                ++indent;
            else if (c == QLatin1Char('\t'))
                indent += 4;
            else
                break;
        }
        QString text = trimmed;
        if (text.startsWith(QLatin1String("root((")) && text.endsWith(QLatin1String("))")))
            text = text.mid(6).chopped(2);

        MermaidData::TreeNode node;
        node.text = text;
        while (!stack.isEmpty() && stack.last().first >= indent)
            stack.removeLast();
        if (stack.isEmpty()) {
            out.mindmapRoots.append(node);
            stack.append({indent, &out.mindmapRoots.last()});
        } else {
            stack.last().second->children.append(node);
            stack.append({indent, &stack.last().second->children.last()});
        }
    }
    out.parseable = sawHeader && !out.mindmapRoots.isEmpty();
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Timeline / Journey (shared shape: `title` + section/entry lines)
// ---------------------------------------------------------------------------

static bool parseTimeline(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    QString currentSection;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("timeline"))) {
            sawHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1String("title "))) {
            out.timelineTitle = line.mid(6).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1String(": "))) {
            out.timelineEntries.append({currentSection, line.mid(2).trimmed()});
            sawContent = true;
        } else if (line.startsWith(QLatin1String(":"))) {
            out.timelineEntries.append({currentSection, line.mid(1).trimmed()});
            sawContent = true;
        } else {
            // A combined `period : event` line sets the current section AND
            // adds an entry; a bare line only sets the section. The combined
            // form is what mermaid's own docs use.
            const int colon = line.indexOf(QLatin1String(" : "));
            if (colon > 0) {
                currentSection = line.left(colon).trimmed();
                const QString event = line.mid(colon + 3).trimmed();
                if (!event.isEmpty()) {
                    out.timelineEntries.append({currentSection, event});
                    sawContent = true;
                }
            } else {
                currentSection = line;
            }
        }
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

static bool parseJourney(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    QString currentSection;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("journey"))) {
            sawHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1String("title "))) {
            out.journeyTitle = line.mid(6).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1String("section "))) {
            currentSection = line.mid(8).trimmed();
            continue;
        }
        // `Task: 5: Me, Team`
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString task = line.left(colon).trimmed();
        const QString rest = line.mid(colon + 1).trimmed();
        const int sc = rest.indexOf(':');
        if (sc <= 0)
            continue;
        const QString score = rest.left(sc).trimmed();
        const QString actors = rest.mid(sc + 1).trimmed();
        out.journeyEntries.append({currentSection, task, score, actors});
        sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Quadrant
// ---------------------------------------------------------------------------

static bool parseQuadrant(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("quadrantChart"))) {
            sawHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1String("title "))) {
            out.quadTitle = line.mid(6).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1String("x-axis "))) {
            const QString axis = line.mid(7).trimmed();
            const int arrow = axis.indexOf(QLatin1String(" --> "));
            if (arrow > 0) {
                out.quadXLeft = axis.left(arrow).trimmed();
                out.quadXRight = axis.mid(arrow + 5).trimmed();
            }
            continue;
        }
        if (line.startsWith(QLatin1String("y-axis "))) {
            const QString axis = line.mid(7).trimmed();
            const int arrow = axis.indexOf(QLatin1String(" --> "));
            if (arrow > 0) {
                out.quadYBottom = axis.left(arrow).trimmed();
                out.quadYTop = axis.mid(arrow + 5).trimmed();
            }
            continue;
        }
        if (line.startsWith(QLatin1String("quadrant-"))) {
            const int space = line.indexOf(' ');
            if (space < 0)
                continue;
            const QString q = line.left(space).mid(9).trimmed();
            const QString label = line.mid(space + 1).trimmed();
            if (q == QLatin1String("1")) out.quadQ1 = label;
            else if (q == QLatin1String("2")) out.quadQ2 = label;
            else if (q == QLatin1String("3")) out.quadQ3 = label;
            else if (q == QLatin1String("4")) out.quadQ4 = label;
            continue;
        }
        // `Label: [x, y]`
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString label = line.left(colon).trimmed();
        const QString rest = line.mid(colon + 1).trimmed();
        static const QRegularExpression re(QStringLiteral(R"(^\[([^,\]]+),\s*([^\]]+)\]$)"));
        const QRegularExpressionMatch m = re.match(rest);
        if (!m.hasMatch())
            continue;
        out.quadPoints.append({label, m.captured(1).trimmed(), m.captured(2).trimmed()});
        sawContent = true;
    }
    out.parseable = sawHeader;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — Sankey
// ---------------------------------------------------------------------------

static bool parseSankey(const QString &diagram, MermaidData &out)
{
    bool sawHeader = false;
    bool sawContent = false;
    for (const QString &rawLine : diagram.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("sankey"))) {
            sawHeader = true;
            continue;
        }
        const QStringList parts = splitCsv(line);
        if (parts.size() < 3)
            continue;
        out.sankeyLinks.append({parts[0], parts[1], parts[2]});
        sawContent = true;
    }
    out.parseable = sawHeader && sawContent;
    return out.parseable;
}

// ---------------------------------------------------------------------------
// Mermaid — dispatch
// ---------------------------------------------------------------------------

bool parseMermaid(const QString &diagram, MermaidData &out)
{
    out.source = diagram;
    QString body = diagram;
    stripFrontmatter(body);
    out.type = detectMermaidType(body);
    switch (out.type) {
    case MermaidType::Pie:        return parsePie(diagram, out);
    case MermaidType::Flowchart:  return parseFlowchart(diagram, out);
    case MermaidType::Sequence:   return parseSequence(diagram, out);
    case MermaidType::Gantt:      return parseGantt(diagram, out);
    case MermaidType::State:      return parseState(diagram, out);
    case MermaidType::Mindmap:    return parseMindmap(diagram, out);
    case MermaidType::Timeline:   return parseTimeline(diagram, out);
    case MermaidType::Journey:    return parseJourney(diagram, out);
    case MermaidType::Quadrant:   return parseQuadrant(diagram, out);
    case MermaidType::Sankey:     return parseSankey(diagram, out);
    case MermaidType::Class:
    case MermaidType::ER:
    case MermaidType::Unknown:
    default:
        // Class and ER have relational sub-tables that are not worth
        // reconstructing; fall back to raw source.
        return false;
    }
}

} // namespace ChartSource
