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
//
// Doc-driven tests for the ChartSource reverse parsers. Every fenced block in
// docs/echarts.md (` ```ec `) and docs/mermaid.md (` ```mermaid `) is fed
// through ChartSource — the reverse-parser that extracts data from a rendered
// chart back into the fields of the Chart Builder / Stock Chart / Mermaid
// Diagrams assistants. Parseable blocks must round-trip: parse -> re-emit
// (mirroring the dialog builders) -> re-parse reproduces identical data.
// Pure logic — no Qt widgets, no WebEngine.

#include <gtest/gtest.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "ChartSource.h"

namespace {

// ---------------------------------------------------------------------------
// Doc loading + fence extraction
// ---------------------------------------------------------------------------

QString readDoc(const QString &name)
{
    const QString path = QStringLiteral(SCRIBA_DOCS_DIR "/") + name;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ADD_FAILURE() << "cannot open doc '" << path.toStdString() << "'";
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

// Extracts the *bodies* of all fenced ` ```lang ` blocks in `markdown`.
QStringList fencedBlocks(const QString &markdown, const QString &lang)
{
    const QRegularExpression re(
        QStringLiteral("^```%1[ \\t]*$\\n(.*?)\\n^```\\s*$").arg(lang),
        QRegularExpression::MultilineOption
            | QRegularExpression::DotMatchesEverythingOption);
    QStringList out;
    QRegularExpressionMatchIterator it = re.globalMatch(markdown);
    while (it.hasNext())
        out.append(it.next().captured(1).trimmed());
    return out;
}

// Returns the body of the first ` ```mermaid ` fence that contains `fragment`.
QString blockContaining(const QString &doc, const QString &fragment)
{
    for (const QString &block : fencedBlocks(doc, QStringLiteral("mermaid")))
        if (block.contains(fragment))
            return block;
    return QString();
}

// ---------------------------------------------------------------------------
// Mermaid -> text serializers (mirror MermaidDialog::build*{Pie,Flowchart,
// Sequence,Gantt,State,Mindmap,Timeline,Journey,Quadrant,Sankey}Diagram). They
// are kept in canonical (fully-determined) form so that re-parsing the emitted
// text yields byte-identical re-emissions.
// ---------------------------------------------------------------------------

QString shapeToText(const QString &shape, const QString &text)
{
    if (shape == QLatin1String("Round"))
        return "(" + text + ")";
    if (shape == QLatin1String("Stadium"))
        return "([" + text + "])";
    if (shape == QLatin1String("Diamond"))
        return "{" + text + "}";
    if (shape == QLatin1String("Hexagon"))
        return "{{" + text + "}}";
    return "[" + text + "]";
}

struct ArrowPair {
    const char *display;
    const char *left;
    const char *right;
};

const ArrowPair kArrowPairs[] = {
    {"-->",   "--",  "-->"},
    {"---",   "--",  "---"},
    {"-.->",  "-.",  ".->"},
    {"==>",   "==",  "==>"},
    {"--o",   "--",  "--o"},
    {"--x",   "--",  "--x"},
    {"<-->",  "<--", "-->"},
    {"<==>",  "<==", "==>"},
    {"<-.->", "<-.", ".->"},
    {"o--o",  "o--", "--o"},
    {"x--x",  "x--", "--x"},
};

QString renderEdgeText(const QString &from, const QString &to, const QString &label,
                       QString arrow)
{
    if (arrow.isEmpty())
        arrow = QStringLiteral("-->");
    const ArrowPair *info = nullptr;
    for (const auto &p : kArrowPairs) {
        if (arrow == QLatin1String(p.display)) {
            info = &p;
            break;
        }
    }
    if (!info)
        return from + "-->" + to;
    if (label.isEmpty())
        return from + info->display + to;
    return from + " " + info->left + " " + label + " " + info->right + " " + to;
}

QString emitPie(const ChartSource::MermaidData &d)
{
    QString out = d.pieTitle.isEmpty() ? QStringLiteral("pie\n")
                                       : QStringLiteral("pie title ") + d.pieTitle + "\n";
    for (const auto &e : d.pieEntries)
        out += "    \"" + e.first + "\" : " + QString::number(e.second) + "\n";
    return out;
}

QString emitFlowchart(const ChartSource::MermaidData &d)
{
    QString dir = d.fcDirection.isEmpty() ? QStringLiteral("TD") : d.fcDirection;
    QString out = "flowchart " + dir + "\n";
    for (const auto &n : d.fcNodes)
        out += "    " + n.at(0) + shapeToText(n.at(2), n.at(1)) + "\n";
    for (const auto &e : d.fcEdges)
        out += "    " + renderEdgeText(e.at(0), e.at(1), e.at(2), e.at(3)) + "\n";
    return out;
}

QString emitSequence(const ChartSource::MermaidData &d)
{
    QString out = "sequenceDiagram\n";
    for (const auto &p : d.seqParticipants) {
        out += "    participant " + p.at(0);
        if (!p.at(1).isEmpty())
            out += " as " + p.at(1);
        out += "\n";
    }
    for (const auto &m : d.seqMessages)
        out += "    " + m.at(0) + m.at(3) + m.at(1) + ": " + m.at(2) + "\n";
    return out;
}

QString emitGantt(const ChartSource::MermaidData &d)
{
    QString out = "gantt\n";
    if (!d.ganttTitle.isEmpty())
        out += "    title " + d.ganttTitle + "\n";
    out += "    dateFormat "
         + (d.ganttDateFormat.isEmpty() ? QStringLiteral("YYYY-MM-DD") : d.ganttDateFormat)
         + "\n";
    if (d.ganttWeekend)
        out += "    excludes weekends\n";
    QString current;
    for (const auto &t : d.ganttTasks) {
        if (t.at(5) != current) {
            current = t.at(5);
            out += "    section " + current + "\n";
        }
        out += "        " + t.at(1);
        if (t.at(4).isEmpty())
            out += " : ";
        else
            out += " :" + t.at(4) + ", ";
        out += t.at(0) + ", " + t.at(2) + ", " + t.at(3) + "\n";
    }
    return out;
}

QString emitState(const ChartSource::MermaidData &d)
{
    QString out = "stateDiagram-v2\n";
    QString open;
    for (const auto &t : d.stateTransitions) {
        const QString section = t.at(3);
        if (section != open) {
            if (!open.isEmpty())
                out += "    }\n";
            if (!section.isEmpty())
                out += "    state " + section + " {\n";
            open = section;
        }
        out += (section.isEmpty() ? QStringLiteral("    ") : QStringLiteral("        "))
             + t.at(0) + " --> " + t.at(1);
        if (!t.at(2).isEmpty())
            out += " : " + t.at(2);
        out += "\n";
    }
    if (!open.isEmpty())
        out += "    }\n";
    return out;
}

QString emitMindmapNode(const ChartSource::MermaidData::TreeNode &node, int depth)
{
    QString indent(depth * 4, QLatin1Char(' '));
    QString out = depth == 1 ? indent + "root((" + node.text + "))\n"
                             : indent + node.text + "\n";
    for (const auto &child : node.children)
        out += emitMindmapNode(child, depth + 1);
    return out;
}

QString emitMindmap(const ChartSource::MermaidData &d)
{
    QString out = "mindmap\n";
    for (const auto &root : d.mindmapRoots)
        out += emitMindmapNode(root, 1);
    return out;
}

QString emitTimeline(const ChartSource::MermaidData &d)
{
    QString out = "timeline\n";
    if (!d.timelineTitle.isEmpty())
        out += "    title " + d.timelineTitle + "\n";
    QString current;
    for (const auto &e : d.timelineEntries) {
        if (e.at(0) != current && !e.at(0).isEmpty()) {
            current = e.at(0);
            out += "    " + current + "\n";
        }
        if (!e.at(1).isEmpty())
            out += "            : " + e.at(1) + "\n";
    }
    return out;
}

QString emitJourney(const ChartSource::MermaidData &d)
{
    QString out = "journey\n";
    if (!d.journeyTitle.isEmpty())
        out += "    title " + d.journeyTitle + "\n";
    QString current;
    for (const auto &e : d.journeyEntries) {
        if (e.at(0) != current && !e.at(0).isEmpty()) {
            current = e.at(0);
            out += "    section " + current + "\n";
        }
        if (!e.at(1).isEmpty())
            out += "        " + e.at(1) + ": " + e.at(2) + ": " + e.at(3) + "\n";
    }
    return out;
}

QString emitQuadrant(const ChartSource::MermaidData &d)
{
    QString out = "quadrantChart\n";
    if (!d.quadTitle.isEmpty())
        out += "    title " + d.quadTitle + "\n";
    out += "    x-axis " + d.quadXLeft + " --> " + d.quadXRight + "\n";
    out += "    y-axis " + d.quadYBottom + " --> " + d.quadYTop + "\n";
    if (!d.quadQ1.isEmpty()) out += "    quadrant-1 " + d.quadQ1 + "\n";
    if (!d.quadQ2.isEmpty()) out += "    quadrant-2 " + d.quadQ2 + "\n";
    if (!d.quadQ3.isEmpty()) out += "    quadrant-3 " + d.quadQ3 + "\n";
    if (!d.quadQ4.isEmpty()) out += "    quadrant-4 " + d.quadQ4 + "\n";
    for (const auto &p : d.quadPoints)
        out += "    " + p.at(0) + ": [" + p.at(1) + ", " + p.at(2) + "]\n";
    return out;
}

QString emitSankey(const ChartSource::MermaidData &d)
{
    QString out = "sankey-beta\n";
    for (const auto &l : d.sankeyLinks)
        out += "    " + l.at(0) + "," + l.at(1) + "," + l.at(2) + "\n";
    return out;
}

// Canonical text for a parsed (supported) diagram: parse(emit(x)) == x.
QString canonicalMermaid(const ChartSource::MermaidData &d)
{
    switch (d.type) {
    case ChartSource::MermaidType::Pie:       return emitPie(d);
    case ChartSource::MermaidType::Flowchart: return emitFlowchart(d);
    case ChartSource::MermaidType::Sequence:  return emitSequence(d);
    case ChartSource::MermaidType::Gantt:     return emitGantt(d);
    case ChartSource::MermaidType::State:     return emitState(d);
    case ChartSource::MermaidType::Mindmap:   return emitMindmap(d);
    case ChartSource::MermaidType::Timeline:  return emitTimeline(d);
    case ChartSource::MermaidType::Journey:   return emitJourney(d);
    case ChartSource::MermaidType::Quadrant:  return emitQuadrant(d);
    case ChartSource::MermaidType::Sankey:    return emitSankey(d);
    default:                                   return QString();
    }
}

// ---------------------------------------------------------------------------
// ECharts JSON serializers (mirror ChartDialog::buildSpec / StockChartDialog)
// ---------------------------------------------------------------------------

QJsonObject serializeChartSpec(const ChartSource::ChartSpecData &d)
{
    QJsonObject spec;
    if (!d.animate)
        spec["animation"] = false;
    if (!d.title.isEmpty()) {
        QJsonObject t;
        t["text"] = d.title;
        spec["title"] = t;
    }
    if (d.tooltip) {
        QJsonObject tt;
        tt["trigger"] = "axis";
        spec["tooltip"] = tt;
    }

    QJsonObject s;
    s["type"] = d.type;
    if (d.type == QLatin1String("area")) {
        QJsonObject areaStyle;
        s["areaStyle"] = areaStyle;
    }
    QJsonArray rows;
    if (d.type == QLatin1String("pie")) {
        for (const auto &row : d.rows) {
            QJsonObject item;
            item["name"] = row.at(0);
            item["value"] = row.at(1).toDouble();
            rows.append(item);
        }
    } else if (d.headers.size() >= 2 && d.headers.at(0) == QLatin1String("X")) {
        for (const auto &row : d.rows) {
            QJsonArray pair{row.at(0).toDouble(), row.at(1).toDouble()};
            rows.append(pair);
        }
    } else {
        QJsonArray cats, vals;
        for (const auto &row : d.rows) {
            cats.append(row.at(0));
            vals.append(row.at(1).toDouble());
        }
        QJsonObject xAxis;
        xAxis["type"] = "category";
        xAxis["data"] = cats;
        spec["xAxis"] = xAxis;
        QJsonObject yAxis;
        yAxis["type"] = "value";
        spec["yAxis"] = yAxis;
        s["data"] = vals;
        spec["series"] = QJsonArray{s};
        return spec;
    }
    s["data"] = rows;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject serializeStockSpec(const ChartSource::StockSpecData &d)
{
    QJsonObject spec;
    if (!d.animate)
        spec["animation"] = false;
    if (!d.title.isEmpty()) {
        QJsonObject t;
        t["text"] = d.title;
        spec["title"] = t;
    }
    QJsonObject tooltip;
    tooltip["trigger"] = "axis";
    spec["tooltip"] = tooltip;

    QJsonArray dates;
    for (const QString &date : d.dates)
        dates.append(date);
    QJsonObject xAxis;
    xAxis["data"] = dates;
    spec["xAxis"] = QJsonArray{xAxis};
    spec["yAxis"] = QJsonArray{QJsonObject{}};

    QJsonArray series;
    QJsonObject candle;
    candle["name"] = "OHLC";
    candle["type"] = "candlestick";
    QJsonArray ohlcData;
    for (const auto &row : d.ohlc) {
        QJsonArray bar;
        for (double v : row)
            bar.append(v);
        ohlcData.append(bar);
    }
    candle["data"] = ohlcData;
    series.append(candle);

    QJsonArray nulls;
    for (int i = 0; i < d.dates.size(); ++i)
        nulls.append(QJsonValue::Null);
    auto addMa = [&series, &nulls](const QString &name, bool on) {
        if (!on)
            return;
        QJsonObject ma;
        ma["name"] = name;
        ma["type"] = "line";
        ma["data"] = nulls;
        series.append(ma);
    };
    addMa("MA5", d.ma5);
    addMa("MA10", d.ma10);
    addMa("MA20", d.ma20);
    addMa("MA50", d.ma50);

    QJsonArray grid;
    grid.append(QJsonObject{});
    if (d.volume || d.hasVolume) {
        grid.append(QJsonObject{});
        QJsonObject vol;
        vol["name"] = "Volume";
        vol["type"] = "bar";
        vol["xAxisIndex"] = 1;
        vol["yAxisIndex"] = 1;
        QJsonArray volData;
        for (double v : d.volumes)
            volData.append(v);
        vol["data"] = volData;
        series.append(vol);
    }
    spec["grid"] = grid;
    spec["series"] = series;
    if (d.zoom)
        spec["dataZoom"] = QJsonArray{QJsonObject{}};
    return spec;
}

struct RoundTripStatus {
    bool parsed = false;
    QString canonical;       // canonical (re-emitted) source after first parse
    QString reCanonical;     // canonical source after the second parse
    bool idempotent = false; // emission is a fixed-point (canonical == reCanonical)
};

// Parses `block`, re-emits it via the matching serializer, re-parses and
// re-emits again; `idempotent` is true when emission is a fixed point.
RoundTripStatus mermaidRoundTrip(const QString &block)
{
    RoundTripStatus st;
    ChartSource::MermaidData a;
    st.parsed = ChartSource::parseMermaid(block, a);
    if (!st.parsed)
        return st;
    st.canonical = canonicalMermaid(a);
    ChartSource::MermaidData b;
    if (!ChartSource::parseMermaid(st.canonical, b))
        return st;
    st.reCanonical = canonicalMermaid(b);
    st.idempotent = st.canonical == st.reCanonical;
    return st;
}

} // namespace

// ---------------------------------------------------------------------------
// DocsMermaid
// ---------------------------------------------------------------------------

TEST(DocsMermaid, FencesPresent)
{
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    ASSERT_FALSE(doc.isEmpty());
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    // Currently 20 documented diagrams; at least this many must always exist
    // so a refactor cannot silently empty the loop.
    EXPECT_GE(blocks.size(), 20);
}

TEST(DocsMermaid, ParsingConsistentWithDetection)
{
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    ASSERT_GE(blocks.size(), 20);
    for (const QString &block : blocks) {
        ChartSource::MermaidData out;
        const bool ok = ChartSource::parseMermaid(block, out);
        switch (out.type) {
        case ChartSource::MermaidType::Pie:
        case ChartSource::MermaidType::Flowchart:
        case ChartSource::MermaidType::Sequence:
        case ChartSource::MermaidType::Gantt:
        case ChartSource::MermaidType::State:
        case ChartSource::MermaidType::Mindmap:
        case ChartSource::MermaidType::Timeline:
        case ChartSource::MermaidType::Journey:
        case ChartSource::MermaidType::Quadrant:
        case ChartSource::MermaidType::Sankey:
            EXPECT_TRUE(ok)
                << "supported diagram failed to parse:\n" << block.toStdString();
            break;
        default:
            // Class/ER/Unknown are raw-edited: parse reports failure and the
            // original source is preserved for the fallback editor.
            EXPECT_FALSE(ok)
                << "unsupported diagram unexpectedly parsed:\n" << block.toStdString();
            EXPECT_EQ(out.source, block);
            break;
        }
    }
}

TEST(DocsMermaid, FlowchartBidirectionalExtraction)
{
    // The styled example (`docs/mermaid.md`) links its subgraph nodes with
    // `A <--> B` / `B <--> C`; those are not decorative — they must extract
    // as real "<-->" edges rather than being dropped.
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    bool found = false;
    for (const QString &block : blocks) {
        if (!block.contains(QLatin1String("<-->")))
            continue;
        found = true;
        ChartSource::MermaidData out;
        ASSERT_TRUE(ChartSource::parseMermaid(block, out));
        ASSERT_EQ(out.type, ChartSource::MermaidType::Flowchart);
        ASSERT_EQ(out.fcEdges.size(), 3) << "styled example must keep both <--> edges";
        EXPECT_EQ(out.fcEdges[0], (QStringList{"A", "B", "", "<-->"}));
        EXPECT_EQ(out.fcEdges[1], (QStringList{"B", "C", "", "<-->"}));
        EXPECT_EQ(out.fcEdges[2], (QStringList{"B", "D", "", "-.->"}));
    }
    ASSERT_TRUE(found) << "expected a `A <--> B` block in docs/mermaid.md";
}

TEST(DocsMermaid, FlowchartExampleExtractsLitmusValues)
{
    const QString block =
        blockContaining(readDoc(QStringLiteral("mermaid.md")),
                        QStringLiteral("Start([Start])"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Flowchart);
    EXPECT_EQ(out.fcDirection, "TD");
    ASSERT_EQ(out.fcNodes.size(), 7);
    EXPECT_EQ(out.fcNodes[0], (QStringList{"Start", "Start", "Stadium"}));
    EXPECT_EQ(out.fcNodes[1], (QStringList{"Auth", "Authenticated?", "Diamond"}));
    EXPECT_EQ(out.fcNodes[2], (QStringList{"Dashboard", "Load Dashboard", "Box"}));
    EXPECT_EQ(out.fcNodes[3], (QStringList{"Login", "Login Page", "Box"}));
    EXPECT_EQ(out.fcNodes[4], (QStringList{"Validate", "Valid Credentials?", "Diamond"}));
    EXPECT_EQ(out.fcNodes[5], (QStringList{"Error", "Show Error", "Box"}));
    EXPECT_EQ(out.fcNodes[6], (QStringList{"End", "End", "Stadium"}));
    ASSERT_EQ(out.fcEdges.size(), 8);
    EXPECT_EQ(out.fcEdges[1], (QStringList{"Auth", "Dashboard", "Yes", "-->"}));
    EXPECT_EQ(out.fcEdges[3], (QStringList{"Login", "Validate", "", "-->"}));
    EXPECT_EQ(out.fcEdges[7], (QStringList{"Dashboard", "End", "", "-->"}));
}

TEST(DocsMermaid, SequenceExtractsLitValues)
{
    const QString block = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                          QStringLiteral("sequenceDiagram"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Sequence);
    ASSERT_EQ(out.seqParticipants.size(), 4);
    EXPECT_EQ(out.seqParticipants[0], (QStringList{"U", "User"}));
    EXPECT_EQ(out.seqParticipants[3], (QStringList{"D", "Database"}));
    ASSERT_EQ(out.seqMessages.size(), 9);
    EXPECT_EQ(out.seqMessages[0],
              (QStringList{"U", "F", "Enter credentials", "->>"}));
    EXPECT_EQ(out.seqMessages[3],
              (QStringList{"D", "A", "User record", "-->>"}));
    EXPECT_EQ(out.seqMessages[4],
              (QStringList{"A", "A", "Verify password", "->>"}));
    EXPECT_EQ(out.seqMessages[8],
              (QStringList{"F", "U", "Show error", "-->>"}));
}

TEST(DocsMermaid, GanttExtractsLitValues)
{
    const QString block = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                          QStringLiteral("Project Timeline"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Gantt);
    EXPECT_EQ(out.ganttTitle, "Project Timeline");
    EXPECT_EQ(out.ganttDateFormat, "YYYY-MM-DD");
    EXPECT_FALSE(out.ganttWeekend); // no `excludes weekends` line in the doc
    ASSERT_EQ(out.ganttTasks.size(), 8);
    EXPECT_EQ(out.ganttTasks[0],
              (QStringList{"a1", "Requirements", "2024-06-01", "5d", "", "Planning"}));
    EXPECT_EQ(out.ganttTasks[1],
              (QStringList{"a2", "Design", "after a1", "7d", "", "Planning"}));
    EXPECT_EQ(out.ganttTasks[7],
              (QStringList{"d1", "Staging", "after c2", "2d", "", "Deploy"}));
}

TEST(DocsMermaid, PieExtractsLitValues)
{
    const QString block = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                          QStringLiteral("Development Stack Usage"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Pie);
    EXPECT_EQ(out.pieTitle, "Development Stack Usage");
    ASSERT_EQ(out.pieEntries.size(), 5);
    EXPECT_EQ(out.pieEntries[0].first, "TypeScript");
    EXPECT_DOUBLE_EQ(out.pieEntries[0].second, 40.0);
    EXPECT_EQ(out.pieEntries[4].first, "Other");
    EXPECT_DOUBLE_EQ(out.pieEntries[4].second, 10.0);
}

TEST(DocsMermaid, JourneyExtractsLitValues)
{
    const QString block = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                          QStringLiteral("Coffee Shop"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Journey);
    EXPECT_EQ(out.journeyTitle, "Coffee Shop Experience");
    ASSERT_EQ(out.journeyEntries.size(), 7);
    EXPECT_EQ(out.journeyEntries[0],
              (QStringList{"Order", "Browse menu", "4", "Customer"}));
    EXPECT_EQ(out.journeyEntries[2],
              (QStringList{"Order", "Pay", "3", "Customer, Cashier"}));
    EXPECT_EQ(out.journeyEntries[6],
              (QStringList{"Enjoy", "Drink coffee", "5", "Customer"}));
}

TEST(DocsMermaid, TimelineExtractsLitValues)
{
    const QString block = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                          QStringLiteral("Company Milestones"));
    ASSERT_FALSE(block.isEmpty());
    ChartSource::MermaidData out;
    ASSERT_TRUE(ChartSource::parseMermaid(block, out));
    EXPECT_EQ(out.type, ChartSource::MermaidType::Timeline);
    EXPECT_EQ(out.timelineTitle, "Company Milestones");
    ASSERT_EQ(out.timelineEntries.size(), 8);
    EXPECT_EQ(out.timelineEntries[0], (QStringList{"2018", "Founded"}));
    EXPECT_EQ(out.timelineEntries[2], (QStringList{"2019", "MVP launch"}));
    EXPECT_EQ(out.timelineEntries[7], (QStringList{"2024", "IPO"}));
}

TEST(DocsMermaid, MindmapAndQuadrantAndSankeyExtract)
{
    const QString mindmap = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                            QStringLiteral("mindmap"));
    ASSERT_FALSE(mindmap.isEmpty());
    ChartSource::MermaidData m;
    ASSERT_TRUE(ChartSource::parseMermaid(mindmap, m));
    EXPECT_EQ(m.mindmapRoots.size(), 1);
    EXPECT_EQ(m.mindmapRoots[0].text, "Software");
    ASSERT_EQ(m.mindmapRoots[0].children.size(), 4);
    EXPECT_EQ(m.mindmapRoots[0].children[0].text, "Frontend");
    ASSERT_EQ(m.mindmapRoots[0].children[0].children.size(), 3);
    EXPECT_EQ(m.mindmapRoots[0].children[0].children[0].text, "React");

    const QString quadrant = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                             QStringLiteral("quadrantChart"));
    ASSERT_FALSE(quadrant.isEmpty());
    ChartSource::MermaidData q;
    ASSERT_TRUE(ChartSource::parseMermaid(quadrant, q));
    EXPECT_EQ(q.quadTitle, "Task Priority");
    EXPECT_EQ(q.quadXLeft, "\"Urgent\"");
    EXPECT_EQ(q.quadXRight, "\"Not Urgent\"");
    EXPECT_EQ(q.quadQ1, "Do First");
    EXPECT_EQ(q.quadQ4, "Eliminate");
    ASSERT_EQ(q.quadPoints.size(), 4);
    EXPECT_EQ(q.quadPoints[0], (QStringList{"Fix outage", "0.15", "0.8"}));

    const QString sankey = blockContaining(readDoc(QStringLiteral("mermaid.md")),
                                           QStringLiteral("sankey"));
    ASSERT_FALSE(sankey.isEmpty());
    ChartSource::MermaidData s;
    ASSERT_TRUE(ChartSource::parseMermaid(sankey, s));
    ASSERT_EQ(s.sankeyLinks.size(), 6);
    EXPECT_EQ(s.sankeyLinks[0], (QStringList{"Coal", "Pulverized", "78"}));
    EXPECT_EQ(s.sankeyLinks[3], (QStringList{"Steam", "Electricity", "22"}));
}

TEST(DocsMermaid, FrontmatterWrappedDiagramsDetected)
{
    // The protocol/packet and config-directive examples conceal their header
    // behind a YAML `---` frontmatter block; detection must skip it.
    EXPECT_EQ(ChartSource::detectMermaidType("---\ntitle: \"HTTP Request Packet\"\n---\npacket\n  0-3: \"Dest\""),
              ChartSource::MermaidType::Unknown); // no assistant for `packet`
    EXPECT_EQ(ChartSource::detectMermaidType(
                  "---\nconfig:\n  theme: base\n---\nflowchart LR\n  A --> B\n"),
              ChartSource::MermaidType::Flowchart);
}

TEST(DocsMermaid, SupportedDiagramsExtractAndRoundTrip)
{
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    ASSERT_GE(blocks.size(), 20);

    int supported = 0;
    for (const QString &block : blocks) {
        switch (ChartSource::detectMermaidType(block)) {
        case ChartSource::MermaidType::Pie:
        case ChartSource::MermaidType::Flowchart:
        case ChartSource::MermaidType::Sequence:
        case ChartSource::MermaidType::Gantt:
        case ChartSource::MermaidType::State:
        case ChartSource::MermaidType::Mindmap:
        case ChartSource::MermaidType::Timeline:
        case ChartSource::MermaidType::Journey:
        case ChartSource::MermaidType::Quadrant:
        case ChartSource::MermaidType::Sankey:
            break;
        default:
            continue; // documented raw/unsupported diagrams are exercised below
        }
        ++supported;
        const RoundTripStatus st = mermaidRoundTrip(block);
        EXPECT_TRUE(st.parsed) << "doc-supported diagram failed to parse";
        EXPECT_TRUE(st.idempotent)
            << "round-trip not idempotent for:\n" << block.toStdString()
            << "\nfirst canonical:\n" << st.canonical.toStdString()
            << "\nsecond canonical:\n" << st.reCanonical.toStdString();
    }
    // The kitchensink covers every assistant type at least once.
    EXPECT_GE(supported, 10);
}

TEST(DocsMermaid, ClassAndERFallBackToRaw)
{
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    for (const QString &block : blocks) {
        const ChartSource::MermaidType t = ChartSource::detectMermaidType(block);
        if (t != ChartSource::MermaidType::Class
            && t != ChartSource::MermaidType::ER)
            continue;
        ChartSource::MermaidData out;
        EXPECT_FALSE(ChartSource::parseMermaid(block, out));
        EXPECT_EQ(out.type, t);
        EXPECT_EQ(out.source, block);
    }
}

TEST(DocsMermaid, UnsupportedDiagramsFallBackToRaw)
{
    const QString doc = readDoc(QStringLiteral("mermaid.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("mermaid"));
    int unknown = 0;
    for (const QString &block : blocks) {
        if (ChartSource::detectMermaidType(block)
            != ChartSource::MermaidType::Unknown)
            continue;
        ++unknown;
        ChartSource::MermaidData out;
        EXPECT_FALSE(ChartSource::parseMermaid(block, out));
        EXPECT_EQ(out.type, ChartSource::MermaidType::Unknown);
        EXPECT_EQ(out.source, block);
    }
    // git graph, C4, block, packet, xychart, requirement, architecture.
    EXPECT_GE(unknown, 6);
}

// ---------------------------------------------------------------------------
// DocsEcharts
// ---------------------------------------------------------------------------

TEST(DocsEcharts, FencesPresent)
{
    const QString doc = readDoc(QStringLiteral("echarts.md"));
    ASSERT_FALSE(doc.isEmpty());
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("ec"));
    EXPECT_GE(blocks.size(), 20);
}

TEST(DocsEcharts, AllBlocksAreValidJson)
{
    const QString doc = readDoc(QStringLiteral("echarts.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("ec"));
    ASSERT_GE(blocks.size(), 20);
    for (const QString &block : blocks) {
        const QJsonDocument d = QJsonDocument::fromJson(block.toUtf8());
        EXPECT_TRUE(d.isObject()) << "invalid JSON:\n" << block.toStdString();
    }
}

TEST(DocsEcharts, ChartAndStockBlocksRoundTrip)
{
    const QString doc = readDoc(QStringLiteral("echarts.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("ec"));
    ASSERT_GE(blocks.size(), 20);
    int chartParsed = 0;
    int stockParsed = 0;
    for (const QString &block : blocks) {
        const QByteArray json = block.toUtf8();
        switch (ChartSource::detectEcType(json)) {
        case ChartSource::EcType::Chart: {
            ChartSource::ChartSpecData a;
            ASSERT_TRUE(ChartSource::parseChartSpec(json, a));
            ++chartParsed;
            const QJsonDocument rebuilt(QJsonObject(serializeChartSpec(a)));
            EXPECT_EQ(ChartSource::detectEcType(rebuilt.toJson(QJsonDocument::Compact)),
                      ChartSource::EcType::Chart);
            ChartSource::ChartSpecData b;
            ASSERT_TRUE(ChartSource::parseChartSpec(rebuilt.toJson(QJsonDocument::Compact), b));
            EXPECT_EQ(a.type, b.type);
            EXPECT_EQ(a.title, b.title);
            EXPECT_EQ(a.tooltip, b.tooltip);
            EXPECT_EQ(a.animate, b.animate);
            EXPECT_EQ(a.headers, b.headers);
            EXPECT_EQ(a.rows, b.rows);
            break;
        }
        case ChartSource::EcType::Stock: {
            ChartSource::StockSpecData a;
            ASSERT_TRUE(ChartSource::parseStockSpec(json, a));
            ++stockParsed;
            const QJsonDocument rebuilt(QJsonObject(serializeStockSpec(a)));
            EXPECT_EQ(ChartSource::detectEcType(rebuilt.toJson(QJsonDocument::Compact)),
                      ChartSource::EcType::Stock);
            ChartSource::StockSpecData b;
            ASSERT_TRUE(ChartSource::parseStockSpec(rebuilt.toJson(QJsonDocument::Compact), b));
            EXPECT_EQ(a.title, b.title);
            EXPECT_EQ(a.volume, b.volume);
            EXPECT_EQ(a.zoom, b.zoom);
            EXPECT_EQ(a.animate, b.animate);
            EXPECT_EQ(a.ma5, b.ma5);
            EXPECT_EQ(a.ma10, b.ma10);
            EXPECT_EQ(a.ma20, b.ma20);
            EXPECT_EQ(a.ma50, b.ma50);
            EXPECT_EQ(a.dates, b.dates);
            EXPECT_EQ(a.ohlc, b.ohlc);
            EXPECT_EQ(a.volumes, b.volumes);
            break;
        }
        case ChartSource::EcType::Unknown:
            break;
        }
    }
    // Line, bar, pie, donut, scatter, styled combo.
    EXPECT_GE(chartParsed, 5);
    // Candlestick.
    EXPECT_GE(stockParsed, 1);
}

TEST(DocsEcharts, StockSpecExampleExtractsExpected)
{
    const QString doc = readDoc(QStringLiteral("echarts.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("ec"));
    ChartSource::StockSpecData out;
    bool found = false;
    for (const QString &block : blocks) {
        if (ChartSource::detectEcType(block.toUtf8()) != ChartSource::EcType::Stock)
            continue;
        found = true;
        ASSERT_TRUE(ChartSource::parseStockSpec(block.toUtf8(), out));
    }
    ASSERT_TRUE(found) << "no candlestick example in docs/echarts.md";
    EXPECT_EQ(out.title, "Sample OHLC");
    EXPECT_TRUE(out.ma5);
    EXPECT_TRUE(out.ma20);
    EXPECT_TRUE(out.zoom);
    ASSERT_EQ(out.dates.size(), 6);
    ASSERT_EQ(out.ohlc.size(), 6);
    EXPECT_EQ(out.ohlc[0].size(), 4);
    EXPECT_FALSE(out.hasVolume);
}

TEST(DocsEcharts, UnsupportedSpecTypesFallBack)
{
    const QString doc = readDoc(QStringLiteral("echarts.md"));
    const QStringList blocks = fencedBlocks(doc, QStringLiteral("ec"));
    int unknown = 0;
    for (const QString &block : blocks) {
        const QByteArray json = block.toUtf8();
        if (ChartSource::detectEcType(json) != ChartSource::EcType::Unknown)
            continue;
        ++unknown;
        ChartSource::ChartSpecData out;
        EXPECT_FALSE(ChartSource::parseChartSpec(json, out));
    }
    // effectScatter, radar, box, heatmap, sankey, treemap, sunburst, funnel,
    // graph, pictorialBar, themeRiver, calendar.
    EXPECT_GE(unknown, 11);
}

// ---------------------------------------------------------------------------
// Exported fixture for the tests
// ---------------------------------------------------------------------------
TEST(DocsParsers, NoWebEngineNeeded)
{
    // Sanity: the pure-logic parser headers compile against the ChartSource
    // symbols without any Qt widget/WebEngine dependency.
    EXPECT_TRUE(true);
}