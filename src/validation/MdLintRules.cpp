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
#include "MdLintRules.h"

#include <QHash>
#include <QSet>

namespace {

struct Def {
    const char *id;
    const char *alias;
    const char *description;
    std::initializer_list<const char *> tags;
    bool aggressive;
    bool warningByDefault = false;
};

const Def kRules[] = {
    {"MD001", "heading-increment", "Heading levels should only increment by one level at a time",
     {"headings"}, false},
    {"MD003", "heading-style", "Heading style", {"headings"}, true},
    {"MD004", "ul-style", "Unordered list style", {"bullet", "ul"}, true},
    {"MD005", "list-indent", "Inconsistent indentation for list items at the same level",
     {"bullet", "indentation", "ul"}, true},
    {"MD007", "ul-indent", "Unordered list indentation", {"bullet", "indentation", "ul"}, true},
    {"MD009", "no-trailing-spaces", "Trailing spaces", {"whitespace"}, false},
    {"MD010", "no-hard-tabs", "Hard tabs", {"hard_tab", "whitespace"}, true},
    {"MD011", "no-reversed-links", "Reversed link syntax", {"links"}, true},
    {"MD012", "no-multiple-blanks", "Multiple consecutive blank lines", {"blank_lines", "whitespace"}, false},
    {"MD013", "line-length", "Line length", {"line_length"}, false},
    {"MD014", "commands-show-output", "Dollar signs used before commands without showing output",
     {"code"}, true},
    {"MD018", "no-missing-space-atx", "No space after hash on atx style heading",
     {"atx", "headings", "spaces"}, false},
    {"MD019", "no-multiple-space-atx", "Multiple spaces after hash on atx style heading",
     {"atx", "headings", "spaces"}, true},
    {"MD020", "no-missing-space-closed-atx", "No space inside hashes on closed atx style heading",
     {"atx_closed", "headings", "spaces"}, true},
    {"MD021", "no-multiple-space-closed-atx", "Multiple spaces inside hashes on closed atx style heading",
     {"atx_closed", "headings", "spaces"}, true},
    {"MD022", "blanks-around-headings", "Headings should be surrounded by blank lines",
     {"blank_lines", "headings"}, true},
    {"MD023", "heading-start-left", "Headings must start at the beginning of the line",
     {"headings", "spaces"}, true},
    {"MD024", "no-duplicate-heading", "Multiple headings with the same content", {"headings"}, false},
    {"MD025", "single-title", "Multiple top-level headings in the same document", {"headings"}, true},
    {"MD026", "no-trailing-punctuation", "Trailing punctuation in heading", {"headings"}, true},
    {"MD027", "no-multiple-space-blockquote", "Multiple spaces after blockquote symbol",
     {"blockquote", "indentation", "whitespace"}, true},
    {"MD028", "no-blanks-blockquote", "Blank line inside blockquote", {"blockquote", "whitespace"}, true},
    {"MD029", "ol-prefix", "Ordered list item prefix", {"ol"}, true},
    {"MD030", "list-marker-space", "Spaces after list markers",
     {"ol", "ul", "whitespace"}, true},
    {"MD031", "blanks-around-fences", "Fenced code blocks should be surrounded by blank lines",
     {"blank_lines", "code"}, true},
    {"MD032", "blanks-around-lists", "Lists should be surrounded by blank lines",
     {"blank_lines", "bullet", "ol", "ul"}, true},
    {"MD033", "no-inline-html", "Inline HTML", {"html"}, true},
    {"MD034", "no-bare-urls", "Bare URL used", {"links", "url"}, true},
    {"MD035", "hr-style", "Horizontal rule style", {"hr"}, true},
    {"MD036", "no-emphasis-as-heading", "Emphasis used instead of a heading", {"emphasis", "headings"}, true},
    {"MD037", "no-space-in-emphasis", "Spaces inside emphasis markers", {"emphasis", "whitespace"}, true},
    {"MD038", "no-space-in-code", "Spaces inside code span elements", {"code", "whitespace"}, true},
    {"MD039", "no-space-in-links", "Spaces inside link text", {"links", "whitespace"}, true},
    {"MD040", "fenced-code-language", "Fenced code blocks should have a language specified",
     {"code", "language"}, true},
    {"MD041", "first-line-heading", "First line in a file should be a top-level heading",
     {"headings"}, true},
    {"MD042", "no-empty-links", "No empty links", {"links"}, true},
    {"MD043", "required-headings", "Required heading structure", {"headings"}, true},
    {"MD044", "proper-names", "Proper names should have the correct capitalization", {"spelling"}, true},
    {"MD045", "no-alt-text", "Images should have alternate text (alt text)",
     {"accessibility", "images"}, true},
    {"MD046", "code-block-style", "Code block style", {"code"}, true},
    {"MD047", "single-trailing-newline", "Files should end with a single newline character",
     {"blank_lines"}, true},
    {"MD048", "code-fence-style", "Code fence style", {"code"}, true},
    {"MD049", "emphasis-style", "Emphasis style", {"emphasis"}, true},
    {"MD050", "strong-style", "Strong style", {"emphasis"}, true},
    {"MD051", "link-fragments", "Link fragments should be valid", {"links"}, true},
    {"MD052", "reference-links-images", "Reference links and images should use a label that is defined",
     {"images", "links"}, true},
    {"MD053", "link-image-reference-definitions",
     "Link and image reference definitions should be needed", {"images", "links"}, true},
    {"MD054", "link-image-style", "Link and image style", {"images", "links"}, true},
    {"MD055", "table-pipe-style", "Table pipe style", {"table"}, true},
    {"MD056", "table-column-count", "Table column count", {"table"}, true},
    {"MD058", "blanks-around-tables", "Tables should be surrounded by blank lines",
     {"blank_lines", "table"}, true},
    {"MD059", "descriptive-link-text", "Link text should be descriptive",
     {"accessibility", "links"}, true},
    {"MD060", "table-column-style", "Table column style", {"table"}, true},
    // Custom scriba rule (out-of-band id: no collision with upstream MD061+)
    {"MD900", "unmatched-footnote", "Unmatched footnote references", {"footnotes"}, false},
    {"MD901", "no-loose-lists", "List items should not be separated by blank lines (keeps the list tight)",
     {"bullet", "ol", "ul"}, true /*aggressive → off in defaults()*/, true /*warningByDefault*/},
};

} // namespace

namespace MdLintRules {

const QVector<MdLintRule> &all()
{
    static const QVector<MdLintRule> rules = [] {
        QVector<MdLintRule> out;
        for (const auto &d : kRules)
            out.append({QLatin1String(d.id), QLatin1String(d.alias),
                        QLatin1String(d.description),
                        [&d] {
                            QStringList tags;
                            for (const char *t : d.tags)
                                tags << QLatin1String(t);
                            return tags;
                        }(),
                        d.aggressive, d.warningByDefault});
        return out;
    }();
    return rules;
}

QStringList allTags()
{
    static const QStringList tags = [] {
        QSet<QString> seen;
        QStringList out;
        for (const auto &r : all())
            for (const auto &t : r.tags)
                if (!seen.contains(t)) {
                    seen.insert(t);
                    out << t;
                }
        return out;
    }();
    return tags;
}

const MdLintRule *byKey(const QString &key)
{
    const QString k = key.toLower();
    for (const auto &r : all())
        if (r.id.compare(k, Qt::CaseInsensitive) == 0
            || r.alias.compare(k, Qt::CaseInsensitive) == 0)
            return &r;
    for (const auto &tag : allTags())
        if (tag.compare(k, Qt::CaseInsensitive) == 0)
            return reinterpret_cast<const MdLintRule *>(1); // tag marker, never dereferenced
    return nullptr;
}

QStringList rulesForTag(const QString &tag)
{
    QStringList out;
    for (const auto &r : all())
        for (const auto &t : r.tags)
            if (t.compare(tag, Qt::CaseInsensitive) == 0) {
                out << r.id;
                break;
            }
    return out;
}

QVector<MdLintParam> paramsFor(const QString &id)
{
    // Params with their defaults, per rule (markdownlint-compatible names).
    struct P { const char *name; QVariant def; };
    static const QHash<QString, QVector<P>> kParams = {
        {QStringLiteral("MD003"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD004"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD007"), {{"indent", 2}, {"start_indented", false}}},
        {QStringLiteral("MD009"), {{"br_spaces", 2}, {"list_item_empty_lines", false}, {"strict", false}}},
        {QStringLiteral("MD010"), {{"code_blocks", true}}},
        {QStringLiteral("MD012"), {{"maximum", 1}}},
        {QStringLiteral("MD013"),
         {{"line_length", 80}, {"heading_line_length", 0}, {"code_block_line_length", 0},
          {"code_blocks", true}, {"tables", true}, {"headings", true}, {"strict", false},
          {"stern", false}}},
        {QStringLiteral("MD022"), {{"lines_above", 1}, {"lines_below", 1}}},
        {QStringLiteral("MD024"), {{"siblings_only", false}, {"allow_different_nesting", false}}},
        {QStringLiteral("MD025"), {{"level", 1}, {"front_matter_title", QString()}}},
        {QStringLiteral("MD026"), {{"punctuation", QStringLiteral(".,;:!。，；：！")}}},
        {QStringLiteral("MD029"), {{"style", QStringLiteral("one")}}},
        {QStringLiteral("MD030"), {{"ul_multi", 3}, {"ol_multi", 2}}},
        {QStringLiteral("MD031"), {{"list_items", true}}},
        {QStringLiteral("MD033"), {{"allowed_elements", QStringList()}}},
        {QStringLiteral("MD035"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD036"), {{"punctuation", QStringLiteral(".,;:!。，；：！")}}},
        {QStringLiteral("MD040"), {{"allowed_languages", QStringList()}, {"language_only", false}}},
        {QStringLiteral("MD041"), {{"level", 1}, {"front_matter_title", QString()}}},
        {QStringLiteral("MD043"), {{"headings", QStringList()}, {"match_case", false}}},
        {QStringLiteral("MD044"), {{"names", QStringList()}, {"code_blocks", true}, {"html_elements", true}}},
        {QStringLiteral("MD046"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD048"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD049"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD050"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD052"), {{"shortcut_syntax", true}, {"collapsed_syntax", true}}},
        {QStringLiteral("MD053"), {{"shortcut_syntax", true}, {"collapsed_syntax", true}}},
        {QStringLiteral("MD054"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD055"), {{"style", QStringLiteral("consistent")}}},
        {QStringLiteral("MD059"), {{"test", QStringLiteral("markdownlint")}}},
        {QStringLiteral("MD060"), {{"style", QStringLiteral("consistent")}}},
    };
    QVector<MdLintParam> out;
    for (const auto &p : kParams.value(id))
        out.append({p.name, p.def});
    return out;
}

} // namespace MdLintRules
