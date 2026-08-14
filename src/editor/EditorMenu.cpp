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
#include "Editor.h"
#include "spell/GrammarChecker.h"
#include "spell/SpellChecker.h"
#include "StaticHelpers.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>

bool Editor::cursorHasUrlSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression urlRe(R"(^https?://\S+$)");
    return urlRe.match(selected).hasMatch();
}

bool Editor::cursorHasImagePathSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression imgRe(R"(^(\./|\.\.\/|/)?\S+\.(png|jpe?g|gif|svg|webp|bmp)$)", QRegularExpression::CaseInsensitiveOption);
    return imgRe.match(selected).hasMatch();
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // Right-click acts on the clicked position: move the caret there so the
    // spelling/grammar lookup and the context-sensitive actions below match
    // where the user clicked. A live selection is kept (for "Make Link").
    QTextCursor cursor = cursorForPosition(event->pos());
    if (textCursor().hasSelection())
        cursor = textCursor();
    else
        setTextCursor(cursor);

    const SpellHighlighter::WordHit misspelled = misspelledWordAt(cursor);
    if (!misspelled.text.isEmpty()) {
        QMenu *suggestions = menu.addMenu("Spelling: " + misspelled.text);
        const QStringList words = m_spellChecker->suggestions(misspelled.text);
        if (words.isEmpty()) {
            QAction *none = suggestions->addAction("No suggestions");
            none->setEnabled(false);
        } else {
            for (const QString &suggestion : words) {
                QAction *action = suggestions->addAction(suggestion);
                connect(action, &QAction::triggered, this, [this, suggestion, misspelled]() {
                    const QTextBlock block = textCursor().block();
                    QTextCursor replace(document());
                    replace.setPosition(block.position() + misspelled.start);
                    replace.setPosition(block.position() + misspelled.start + misspelled.length,
                                        QTextCursor::KeepAnchor);
                    replace.insertText(suggestion);
                });
            }
        }

        QAction *addAction = menu.addAction("Add to Dictionary: " + misspelled.text);
        connect(addAction, &QAction::triggered, this, [this, misspelled]() {
            if (m_corpusActive)
                m_spellChecker->addCorpusWord(misspelled.text);
            else
                m_spellChecker->addToUserDictionary(misspelled.text);
            m_spellHighlighter->refresh();
        });

        menu.addSeparator();
    }

    // Grammar issues under the cursor: each squiggle becomes a submenu whose
    // title states the justification for the error, with one-click fixes
    // inside. A rule is drawn underneath each submenu.
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->grammarIssuesInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;

        QMenu *grammarMenu = menu.addMenu("Grammar: " + hit.message);
        const int blockNumber = cursor.block().blockNumber();

        if (hit.suggestions.isEmpty()) {
            QAction *none = grammarMenu->addAction("No suggestions");
            none->setEnabled(false);
        } else {
            for (const GrammarChecker::Issue::Suggestion &suggestion : hit.suggestions) {
                QString label;
                switch (suggestion.kind) {
                case GrammarChecker::Issue::SuggestionKind::Replace:
                    label = "Replace with '" + suggestion.text + "'";
                    break;
                case GrammarChecker::Issue::SuggestionKind::Remove:
                    label = "Remove";
                    break;
                case GrammarChecker::Issue::SuggestionKind::InsertAfter:
                    label = "Insert '" + suggestion.text + "' after";
                    break;
                }
                QAction *action = grammarMenu->addAction(label);
                connect(action, &QAction::triggered, this,
                        [this, blockNumber, hit, suggestion]() {
                            const QTextBlock block = document()->findBlockByNumber(blockNumber);
                            if (!block.isValid())
                                return;
                            QTextCursor fix(document());
                            fix.setPosition(block.position() + hit.start);
                            fix.setPosition(block.position() + hit.start + hit.length,
                                            QTextCursor::KeepAnchor);
                            if (suggestion.kind
                                == GrammarChecker::Issue::SuggestionKind::InsertAfter) {
                                fix.setPosition(block.position() + hit.start + hit.length);
                                fix.insertText(suggestion.text);
                            } else if (suggestion.kind
                                       == GrammarChecker::Issue::SuggestionKind::Remove) {
                                fix.removeSelectedText();
                            } else {
                                fix.insertText(suggestion.text);
                            }
                        });
            }
        }

        menu.addSeparator();
    }

    // Broken links under the cursor: an informational entry (there is no
    // automatic fix for a missing file or a malformed URL).
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->linkIssuesInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;
        QAction *brokenLink = menu.addAction("Broken link: " + hit.message);
        brokenLink->setEnabled(false);
        menu.addSeparator();
    }

    // Markdown-consistency issues under the cursor (heading-level skips,
    // duplicate headings, trailing whitespace, ...): an informational entry,
    // same as broken links — there is no single automatic fix.
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->markdownHitsInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;
        QAction *mdIssue = menu.addAction("Markdown: " + hit.message);
        mdIssue->setEnabled(false);
        menu.addSeparator();
    }

    for (QAction *action : m_insertActions)
        menu.addAction(action);

    if (m_mermaidAction)
        menu.addAction(m_mermaidAction);

    CursorContext ctx = detectCursorContext();

    if (ctx == CursorContext::ListItem) {
        menu.addSeparator();
        QAction *indentAction = menu.addAction("Increase Indent");
        connect(indentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString indented = indentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(indented);
        });

        QAction *dedentAction = menu.addAction("Decrease Indent");
        connect(dedentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString outdented = outdentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(outdented);
        });

        static const QRegularExpression taskRe(R"(^\s*[-*+]\s\[)");
        if (taskRe.match(currentLineText()).hasMatch()) {
            QAction *toggleAction = menu.addAction("Toggle Checkbox");
            connect(toggleAction, &QAction::triggered, this, &Editor::toggleCheckbox);
        }
    }

    if (ctx == CursorContext::TableRow) {
        menu.addSeparator();
        QAction *above = menu.addAction("Insert Row Above");
        connect(above, &QAction::triggered, this, [this]() { insertTableRow(true); });

        QAction *below = menu.addAction("Insert Row Below");
        connect(below, &QAction::triggered, this, [this]() { insertTableRow(false); });

        menu.addSeparator();

        QAction *colLeft = menu.addAction("Insert Column Left");
        connect(colLeft, &QAction::triggered, this, [this]() { insertTableCol(true); });

        QAction *colRight = menu.addAction("Insert Column Right");
        connect(colRight, &QAction::triggered, this, [this]() { insertTableCol(false); });

        menu.addSeparator();

        QAction *delRow = menu.addAction("Delete Row");
        connect(delRow, &QAction::triggered, this, &Editor::deleteTableRow);

        QAction *delCol = menu.addAction("Delete Column");
        connect(delCol, &QAction::triggered, this, &Editor::deleteTableCol);
    }

    if (ctx == CursorContext::CodeBlock) {
        menu.addSeparator();
        QAction *langAction = menu.addAction("Change Language...");
        connect(langAction, &QAction::triggered, this, &Editor::changeCodeLanguage);
    }

    if (cursorHasUrlSelection()) {
        menu.addSeparator();
        QAction *makeLink = menu.addAction("Make Link");
        connect(makeLink, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString url = cursor.selectedText();
            cursor.insertText("[](" + url + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, url.size() + 3);
            setTextCursor(cursor);
        });
    }

    if (cursorHasImagePathSelection()) {
        menu.addSeparator();
        QAction *insertImg = menu.addAction("Insert Image");
        connect(insertImg, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString path = cursor.selectedText();
            cursor.insertText("![](" + path + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, path.size() + 4);
            setTextCursor(cursor);
        });
    }

    if (!menu.isEmpty())
        menu.exec(event->globalPos());
}
