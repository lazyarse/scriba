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

#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QToolButton;
class QTimer;
class QVBoxLayout;

// Semi-transparent overlay that floats in the top-right of the editor and
// lists live issue counts (typos, grammar, markdown lint, broken links) for
// the current .md file. It is a child of the editor viewport; the body passes
// mouse events through to the editor (only the [x] button and the per-row
// checkboxes are interactive).
//
// Each top-level row carries a checkbox that filters the corresponding error
// bars in the editor's vertical scrollbar: unchecking a kind dims the row and
// emits filterChanged() so the owner can hide that kind's markers. The state
// is session-only (all kinds checked initially) and survives row rebuilds.
//
// The pane never auto-appears by itself: callers drive it via setRows() +
// showWithTimeout(). A timeout <= 0 keeps it displayed until dismissed by [x]
// (or the timeout, when one is configured). Dismissal emits closeRequested()
// so the owner can stop re-showing it on every keystroke.
class IssueSummaryPane : public QWidget
{
    Q_OBJECT

public:
    enum class Kind { Typos, Grammar, Lint, Links };

    struct Row {
        Kind kind = Kind::Typos;
        QString label;
        int count = 0;
        QColor color;
        // Indent level (0 = top-level row); each level adds a left margin.
        int indentLevel = 0;
    };

    explicit IssueSummaryPane(QWidget *parent = nullptr);

    void setRows(const QVector<Row> &rows);
    // Builds the translucent panel + text styling from the active theme.
    void setTheme(const QColor &background, const QColor &foreground);
    // Shows the pane; timeoutMs <= 0 keeps it visible until dismissed.
    void showWithTimeout(int timeoutMs);

signals:
    void closeRequested();
    // A top-level row's checkbox was toggled: `visible` is true when the
    // kind's scrollbar bars should stay shown, false when they are filtered
    // out. Not emitted for indented sub-rows (they share their header's
    // checkbox).
    void filterChanged(Kind kind, bool visible);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();
    void relayout();
    void onTimeout();
    void applyRowStyle(QLabel *label, Kind kind);

    QVector<Row> m_rows;
    // Checked state per kind, session-only; all kinds start visible. Survives
    // rebuilds so rows that disappear (engine off) and come back keep their
    // filter state.
    QHash<Kind, bool> m_checked = {
        {Kind::Typos, true},
        {Kind::Grammar, true},
        {Kind::Lint, true},
        {Kind::Links, true},
    };
    QColor m_bg = QColor(QStringLiteral("#ffffff"));
    QColor m_fg = QColor(QStringLiteral("#333333"));
    QToolButton *m_closeBtn = nullptr;
    QTimer *m_timer = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    // Row labels in layout order, owned by m_rowsLayout. Reused in place on
    // count-only updates so the pane doesn't tear down and recreate every row
    // widget on each checker update.
    QVector<QLabel *> m_rowLabels;
};