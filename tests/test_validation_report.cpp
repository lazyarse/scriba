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

#include "SpellChecker.h"
#include "StoppardEngine.h"
#include "ValidationReport.h"
#include "MarkdownParser.h"
#include "TestConfig.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

namespace {

QStringList messages(const QVector<ValidationReport::Issue> &issues)
{
    QStringList out;
    for (const auto &issue : issues)
        out << issue.message;
    return out;
}

class ValidationReportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QDir().mkpath(SpellChecker::configDictDir());
        ASSERT_TRUE(m_dir.isValid());
        QFile exists(m_dir.filePath(QStringLiteral("exists.md")));
        ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
        exists.write("# Sub\n");
        exists.close();

        m_docPath = m_dir.filePath(QStringLiteral("doc.md"));
        m_checker = std::make_unique<SpellChecker>();
        ASSERT_TRUE(m_checker->loadLanguage("en_US"));
        m_grammar = std::make_unique<StoppardEngine>();
    }

    // Full synchronous scan of one document (spelling + grammar + links +
    // markdown), returning the rendered markdown as well as the report.
    ValidationReport::DocumentReport scanOne(const QString &text,
                                             QString *rendered = nullptr) const
    {
        ValidationReport report;
        const QVector<ValidationReport::DocumentReport> docs =
            report.scan({{m_docPath, text}}, m_checker.get());
        ValidationReport::DocumentReport doc = docs.value(0);
        doc.issues[ValidationReport::Category::Grammar] =
            ValidationReport::grammarIssuesToLineIssues(text, m_grammar->check(text));
        if (rendered)
            *rendered = ValidationReport::renderMarkdown({doc}, QStringLiteral("2026-08-05T12:00:00"));
        return doc;
    }

    QTemporaryDir m_dir;
    QString m_docPath;
    std::unique_ptr<SpellChecker> m_checker;
    std::unique_ptr<StoppardEngine> m_grammar;
};

// ---- markdown-consistency scan ----

TEST_F(ValidationReportTest, FlagsHeadingLevelSkip)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "# Level one\n### Level three\n"));
    const QStringList msgs = messages(issues);
    ASSERT_TRUE(msgs.contains(QStringLiteral("Heading level skipped (#3 after #1)")))
        << msgs.join("\n").toStdString();
}

TEST_F(ValidationReportTest, FlagsDuplicateHeadings)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "# Title\n## Title\n"));
    const QStringList msgs = messages(issues);
    EXPECT_TRUE(msgs.contains(QStringLiteral("Duplicate heading (anchor #title already used)")))
        << msgs.join("\n").toStdString();
}

TEST_F(ValidationReportTest, FlagsTrailingWhitespace)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "text  \nmore\t\n"));
    const QStringList msgs = messages(issues);
    EXPECT_EQ(2, msgs.count(QStringLiteral("Trailing whitespace")));
}

TEST_F(ValidationReportTest, FlagsConsecutiveBlankLines)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "one\n\n\n\ntwo\n"));
    EXPECT_TRUE(messages(issues).contains(QStringLiteral("Consecutive blank lines (3 or more)")));
}

TEST_F(ValidationReportTest, FlagsOverlongLines)
{
    const QString line = QString(150, QLatin1Char('a'));
    const auto issues = ValidationReport::scanMarkdownIssues(line + QLatin1Char('\n'));
    EXPECT_TRUE(messages(issues).contains(QStringLiteral("Line too long (150 characters)")));
}

TEST_F(ValidationReportTest, FlagsHashHeadingWithoutSpace)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral("#notrendered\n"));
    EXPECT_TRUE(messages(issues).contains(
        QStringLiteral("Heading missing space after '#' (not rendered as a heading)")));
}

TEST_F(ValidationReportTest, FlagsUnmatchedFootnoteReference)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "Some text[^1] and more[^note].\n\n[^note]: the definition\n"));
    const QStringList msgs = messages(issues);
    EXPECT_TRUE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^1]")));
    EXPECT_FALSE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^note]")));
}

TEST_F(ValidationReportTest, SkipsFencedCodeAndFrontMatter)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "# Heading\n"
        "```\n"
        "# code\n"
        "#nospace\n"
        "```\n"
        "# Heading\n"
        "\n\n\n"));
    const QStringList msgs = messages(issues);
    // Only the duplicate heading and the blank-line run survive.
    EXPECT_EQ(1, msgs.count(QStringLiteral("Duplicate heading (anchor #heading already used)")));
    EXPECT_TRUE(msgs.contains(QStringLiteral("Consecutive blank lines (3 or more)")));
    EXPECT_FALSE(msgs.contains(
        QStringLiteral("Heading missing space after '#' (not rendered as a heading)")));
}

TEST_F(ValidationReportTest, SkipsFootnoteLikeTextInInlineCode)
{
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "Use `[^1]` inline and a real one[^2].\n"));
    const QStringList msgs = messages(issues);
    EXPECT_TRUE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^2]")));
    EXPECT_FALSE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^1]")));
}

TEST_F(ValidationReportTest, SetextHeadingIsNotAHeadingSkip)
{
    // A setext H2 after an ATX H1 is a normal level step, not a skip.
    const auto issues = ValidationReport::scanMarkdownIssues(QStringLiteral(
        "# Title\nSubheading\n-------\n"));
    EXPECT_TRUE(messages(issues).isEmpty());
}

TEST_F(ValidationReportTest, ScanHonoursSelectedCategories)
{
    ValidationReport report;
    ValidationReport::ValidationOptions opts;
    opts.categories = {ValidationReport::Category::Links};
    const QVector<ValidationReport::DocumentReport> docs =
        report.scan({{m_docPath,
                      QStringLiteral("recieve\n[bad](missing.md)\n#nope\n\n\n\n")}},
                    m_checker.get(), opts);
    const auto &doc = docs.first();
    EXPECT_TRUE(doc.issues.value(ValidationReport::Category::Spelling).isEmpty());
    EXPECT_TRUE(doc.issues.value(ValidationReport::Category::Markdown).isEmpty());
    EXPECT_FALSE(doc.issues.value(ValidationReport::Category::Links).isEmpty());
}

TEST_F(ValidationReportTest, ScanMarkdownHonoursSelectedSubChecks)
{
    const QString text = QStringLiteral(
        "# Title\n## Title\n"
        "# Level one\n### Level three\n"
        "trailing  \n"
        "\n\n\n");
    const auto onlyDuplicate = ValidationReport::scanMarkdownIssues(
        text, {ValidationReport::MarkdownCheck::DuplicateHeading});
    const QStringList msgs = messages(onlyDuplicate);
    ASSERT_EQ(1, msgs.size()) << msgs.join("\n").toStdString();
    EXPECT_TRUE(msgs.contains(QStringLiteral("Duplicate heading (anchor #title already used)")));

    // An empty sub-check set disables every markdown check.
    const auto none = ValidationReport::scanMarkdownIssues(text, {});
    EXPECT_TRUE(none.isEmpty());
}

TEST_F(ValidationReportTest, RenderMarkdownOmitsUncheckedCategories)
{
    ValidationReport::DocumentReport doc;
    doc.label = QStringLiteral("doc.md");
    doc.issues[ValidationReport::Category::Spelling]
        .append({1, 1, 0, QStringLiteral("recieve"), {}});
    const QString rendered = ValidationReport::renderMarkdown(
        {doc}, QStringLiteral("2026-08-05T12:00:00"),
        {ValidationReport::Category::Spelling});

    EXPECT_TRUE(rendered.contains(QStringLiteral("### Spelling")));
    EXPECT_FALSE(rendered.contains(QStringLiteral("### Grammar")));
    EXPECT_FALSE(rendered.contains(QStringLiteral("### Links & anchors")));
    EXPECT_FALSE(rendered.contains(QStringLiteral("### Markdown consistency")));
    // The summary table keeps only the selected column.
    EXPECT_TRUE(rendered.contains(QStringLiteral("| Document | Spelling |")));
    EXPECT_FALSE(rendered.contains(QStringLiteral("| Grammar |")));
}

// ---- full report ----

TEST_F(ValidationReportTest, ReportFindsAllCategories)
{
    QString rendered;
    const auto doc = scanOne(QStringLiteral(
        "# Intro\n"
        "\n"
        "A typo recieve in this line.\n"
        "And grammar: I has a cat.\n"
        "[bad](missing.md) and [anchor](#nope) and [ok](exists.md) and [good](#intro)\n"),
        &rendered);

    const auto spelling = doc.issues.value(ValidationReport::Category::Spelling);
    const auto grammar = doc.issues.value(ValidationReport::Category::Grammar);
    const auto links = doc.issues.value(ValidationReport::Category::Links);
    const auto markdown = doc.issues.value(ValidationReport::Category::Markdown);

    ASSERT_FALSE(spelling.isEmpty());
    bool foundTypo = false;
    for (const auto &issue : spelling) {
        if (issue.message == QStringLiteral("recieve"))
            foundTypo = true;
    }
    EXPECT_TRUE(foundTypo) << "expected 'recieve' among: "
                           << messages(spelling).join(", ").toStdString();
    // The typo is on line 3.
    EXPECT_EQ(3, spelling[0].line);
    EXPECT_GT(spelling[0].column, 0);

    EXPECT_FALSE(grammar.isEmpty());
    for (const auto &issue : grammar) {
        EXPECT_GE(issue.line, 1);
        EXPECT_GT(issue.line, 0);
    }

    const QStringList linkMsgs = messages(links);
    EXPECT_TRUE(linkMsgs.contains(QStringLiteral("File not found: missing.md")))
        << linkMsgs.join("\n").toStdString();
    EXPECT_TRUE(linkMsgs.contains(QStringLiteral("Heading not found: #nope")))
        << linkMsgs.join("\n").toStdString();

    EXPECT_TRUE(markdown.isEmpty())
        << "clean of markdown issues: " << messages(markdown).join("\n").toStdString();

    ASSERT_TRUE(rendered.contains(QStringLiteral("# Validation Report")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("## doc.md")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("### Spelling")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("### Grammar")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("### Links & anchors")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("### Markdown consistency")));
    EXPECT_TRUE(rendered.contains(QStringLiteral("File not found: missing.md")));
    EXPECT_TRUE(rendered.contains(QLatin1String("Path: `") + m_docPath + QLatin1Char('`')));
    EXPECT_TRUE(rendered.contains(QStringLiteral("| **Total** |")));
}

TEST_F(ValidationReportTest, CleanDocumentReportsNoIssues)
{
    QString rendered;
    const auto doc = scanOne(QStringLiteral(
        "# Intro\n\nThis document is entirely fine.\n"),
        &rendered);
    for (const auto &key : doc.issues.keys())
        EXPECT_TRUE(doc.issues.value(key).isEmpty()) << "unexpected issues in category";
    EXPECT_TRUE(rendered.contains(QStringLiteral("No issues found.")));
}

TEST_F(ValidationReportTest, UntitledTabIsLabelledUntitled)
{
    ValidationReport report;
    const QVector<ValidationReport::DocumentReport> docs =
        report.scan({{QString(), QStringLiteral("# Only a heading\n")}}, nullptr);
    ASSERT_EQ(1, docs.size());
    EXPECT_EQ(QStringLiteral("(Untitled)"), docs.first().label);
}

TEST_F(ValidationReportTest, RenderMarkdownDeepLinksToFindings)
{
    // The summary table's document cell must link to the report's per-file
    // heading and each count cell to that file's category sub-heading, using
    // the same slug/dedup the preview's generateHeadingIds() produces.
    ValidationReport report;
    const QVector<ValidationReport::DocumentReport> docs =
        report.scan({{m_docPath, QStringLiteral("A typo recieve here.\n")}}, m_checker.get());
    const QString out = ValidationReport::renderMarkdown(
        {docs.value(0)}, QStringLiteral("2026-08-05T12:00:00"));

    // Document cell deep link: [doc.md](#docmd) — the `.` in the label is
    // dropped, `d`,`o`,`c`,`m`,`d` remain.
    EXPECT_TRUE(out.contains(QStringLiteral("[doc.md](#docmd)")))
        << out.toStdString();
    // Spelling count cell links to the per-file "Spelling" sub-heading.
    EXPECT_TRUE(out.contains(QStringLiteral("[1](#spelling)")))
        << out.toStdString();
    // The summary and file headings are emitted (anchors that make the links
    // resolve) in the right slug order.
    EXPECT_TRUE(out.contains(QStringLiteral("# Validation Report")));
    EXPECT_TRUE(out.contains(QStringLiteral("## Summary")));
    EXPECT_TRUE(out.contains(QStringLiteral("## doc.md")));
    EXPECT_TRUE(out.contains(QStringLiteral("### Spelling")));
}

TEST_F(ValidationReportTest, RenderMarkdownContextQuoteQuotesSourceLine)
{
    QString rendered;
    const auto doc = scanOne(QStringLiteral("line one\nA typo recieve here.\n"),
                             &rendered);
    ASSERT_FALSE(doc.issues.value(ValidationReport::Category::Spelling).isEmpty());
    // The finding's context is quoted as an inert fenced code block so source
    // content can never inject Markdown structure into the report.
    EXPECT_TRUE(rendered.contains(
        QLatin1String("```text\nA typo recieve here.\n```")))
        << rendered.toStdString();
}

// The regression behind the report "white preview": when a scanned source line
// is itself Markdown (a table cell containing `==…==` highlights, inline
// backticks, fences), quoting it back as live Markdown could produce an
// unbalanced `==` and swallow the rest of the report's render. Quoting it as a
// code block must keep the report well-formed.
TEST_F(ValidationReportTest, ContextQuoteCannotMangleReportMarkdown)
{
    const QString poison =
        QStringLiteral("| Symbol | `--bg` `background-color` | `==highlight==` fill (`--color`) |");
    ValidationReport::DocumentReport doc;
    doc.label = QStringLiteral("poison.md");
    doc.filePath = m_docPath;
    doc.sourceLines = { poison, QStringLiteral("ordinary line") };
    const int col = poison.indexOf(QStringLiteral("==")) + 1; // span lands on the `==`
    doc.issues[ValidationReport::Category::Spelling].append(
        {1, col, 6, QStringLiteral("message"), {}});

    ValidationReport::DocumentReport clean;
    clean.label = QStringLiteral("clean.md");
    clean.filePath = m_docPath;
    clean.sourceLines = { QStringLiteral("A second document.") };

    const QString out = ValidationReport::renderMarkdown(
        {doc, clean}, QStringLiteral("2026-08-05T12:00:00"));
    const QString html = MarkdownParser::toHtml(out);

    // The poisoned line is quoted inertly (it must not become a `<mark>` or an
    // open table), and the highlight is no longer injected as live Markdown.
    EXPECT_EQ(0, html.count(QLatin1String("<mark>"))) << html.toStdString();
    EXPECT_TRUE(html.contains(QLatin1String("<pre"))) << html.toStdString();
    // The following report section still renders its own heading — proof the
    // poisoned line did not swallow the rest into an unclosed block. The
    // clean.md heading must appear AFTER the closed code block, not inside it.
    EXPECT_TRUE(html.contains(QLatin1String("</code></pre>")))
        << html.toStdString();
    EXPECT_TRUE(html.lastIndexOf(QLatin1String("clean.md"))
                > html.lastIndexOf(QLatin1String("</pre>")))
        << html.toStdString();
}

TEST_F(ValidationReportTest, GrammarIssuesGetLineAndColumn)
{
    const QString text = QStringLiteral("line one\nI has a cat.\n");
    const auto issues = ValidationReport::grammarIssuesToLineIssues(text, m_grammar->check(text));
    ASSERT_FALSE(issues.isEmpty());
    for (const auto &issue : issues) {
        EXPECT_EQ(2, issue.line) << "the grammar error sits on line two";
        EXPECT_GT(issue.column, 0);
        EXPECT_FALSE(issue.message.isEmpty());
    }
}

TEST_F(ValidationReportTest, SpellingSuggestionsPopulateTheFixColumn)
{
    const auto doc = scanOne(QStringLiteral("recieve\n"));
    const auto spelling = doc.issues.value(ValidationReport::Category::Spelling);
    ASSERT_FALSE(spelling.isEmpty());
    bool hasFix = false;
    for (const auto &issue : spelling) {
        if (issue.message == QStringLiteral("recieve"))
            hasFix = !issue.suggestion.isEmpty();
    }
    EXPECT_TRUE(hasFix) << "expected a 'receive' suggestion for 'recieve'";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
