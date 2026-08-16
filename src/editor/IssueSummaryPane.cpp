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

#include "IssueSummaryPane.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString rowText(const IssueSummaryPane::Row &row)
{
    return QStringLiteral("<span style=\"color:%1;\">\u25cf</span> %2: <b>%3</b>")
        .arg(row.color.name(), row.label.toHtmlEscaped())
        .arg(row.count);
}

// A textless QCheckBox is only clickable on its 14x14 indicator
// (SE_CheckBoxClickRect collapses to the indicator when there is no text), so
// clicks on the rest of the row strip would fall through to the editor. Make
// the whole rect the hit target.
class RowCheckBox : public QCheckBox
{
public:
    using QCheckBox::QCheckBox;
    bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
};

// The label beside a RowCheckBox forwards clicks to it so the whole row
// toggles; labels without a checkbox keep passing clicks through (the pane's
// default behavior).
class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    using QLabel::QLabel;
    void mousePressEvent(QMouseEvent *event) override
    {
        emit clicked();
        event->accept();
    }
signals:
    void clicked();
};

} // namespace

IssueSummaryPane::IssueSummaryPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("issue-summary-pane"));
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 6, 6, 8);
    outer->setSpacing(4);

    auto *header = new QHBoxLayout;
    header->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("Issues"));
    title->setTextInteractionFlags(Qt::NoTextInteraction);
    m_closeBtn = new QToolButton;
    m_closeBtn->setText(QStringLiteral("\u2715"));
    m_closeBtn->setAutoRaise(true);
    m_closeBtn->setToolTip(QStringLiteral("Close"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() {
        hide();
        emit closeRequested();
    });
    header->addWidget(title, 1);
    header->addWidget(m_closeBtn);
    outer->addLayout(header);

    m_rowsLayout = new QVBoxLayout;
    m_rowsLayout->setSpacing(2);
    outer->addLayout(m_rowsLayout);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &IssueSummaryPane::onTimeout);

    hide();
}

void IssueSummaryPane::setTheme(const QColor &background, const QColor &foreground)
{
    m_bg = background;
    m_fg = foreground;
    QColor hover = foreground;
    hover.setAlpha(40);
    setStyleSheet(QString(
        "#issue-summary-pane QLabel { color: %1; background: transparent; }"
        "#issue-summary-pane QToolButton { color: %1; background: transparent;"
        "  border: none; font-size: 11pt; }"
        "#issue-summary-pane QToolButton:hover {"
        "  background-color: rgba(%2,%3,%4,%5); border-radius: 4px; }"
        "#issue-summary-pane QCheckBox { background: transparent; spacing: 4px; }"
        "#issue-summary-pane QCheckBox::indicator {"
        "  width: 12px; height: 12px;"
        "  border: 1px solid rgba(%2,%3,%4,90); border-radius: 3px;"
        "  background: transparent; }"
        "#issue-summary-pane QCheckBox::indicator:hover { border-color: %1; }"
        "#issue-summary-pane QCheckBox::indicator:checked {"
        "  border-color: %1; background-color: rgba(%2,%3,%4,70); }")
        .arg(foreground.name())
        .arg(hover.red()).arg(hover.green()).arg(hover.blue()).arg(hover.alpha()));
}

void IssueSummaryPane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QColor bg = m_bg;
    bg.setAlpha(205);
    p.fillRect(rect(), bg);
}

void IssueSummaryPane::setRows(const QVector<Row> &rows)
{
    // Structural identity: same row count with the same kinds, labels and
    // indent levels in order. Counts don't affect structure, so a count-only
    // update reuses the existing QLabels in place instead of deleting and
    // recreating every row.
    bool sameStructure = rows.size() == m_rows.size();
    if (sameStructure) {
        for (int i = 0; i < rows.size(); ++i) {
            if (rows.at(i).kind != m_rows.at(i).kind
                || rows.at(i).label != m_rows.at(i).label
                || rows.at(i).indentLevel != m_rows.at(i).indentLevel) {
                sameStructure = false;
                break;
            }
        }
    }
    if (sameStructure) {
        for (int i = 0; i < m_rowLabels.size(); ++i) {
            m_rowLabels.at(i)->setText(rowText(rows.at(i)));
            applyRowStyle(m_rowLabels.at(i), rows.at(i).kind);
        }
        m_rows = rows;
        relayout();
        return;
    }
    m_rows = rows;
    rebuild();
}

void IssueSummaryPane::showWithTimeout(int timeoutMs)
{
    m_timer->stop();
    if (timeoutMs > 0)
        m_timer->start(timeoutMs);
    show();
    raise();
}

void IssueSummaryPane::mousePressEvent(QMouseEvent *event)
{
    // Body clicks fall through to the editor below (only [x] is interactive).
    event->ignore();
}

void IssueSummaryPane::rebuild()
{
    while (QLayoutItem *item = m_rowsLayout->takeAt(0)) {
        if (QLayout *sub = item->layout()) {
            // Deleting a layout does NOT delete its widgets (they stay
            // parented to the pane), so drop them explicitly.
            while (QLayoutItem *child = sub->takeAt(0)) {
                if (QWidget *w = child->widget())
                    delete w;
                delete child;
            }
        } else if (QWidget *w = item->widget()) {
            delete w;
        }
        delete item;
    }
    m_rowLabels.clear();
    // Width reserved for a row checkbox so indented sub-rows without one stay
    // aligned under their header's label.
    const int checkboxWidth = style()->pixelMetric(QStyle::PM_IndicatorWidth) + 4;
    for (const Row &row : m_rows) {
        auto *hbox = new QHBoxLayout;
        hbox->setSpacing(0);
        hbox->setContentsMargins(row.indentLevel * 14, 0, 0, 0);
        if (row.indentLevel == 0) {
            auto *box = new RowCheckBox;
            box->setChecked(m_checked.value(row.kind, true));
            box->setCursor(Qt::PointingHandCursor);
            box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(box, &QCheckBox::toggled, this, [this, kind = row.kind](bool on) {
                m_checked[kind] = on;
                for (int i = 0; i < m_rows.size(); ++i) {
                    if (m_rows.at(i).kind == kind && m_rowLabels.value(i))
                        applyRowStyle(m_rowLabels.at(i), kind);
                }
                emit filterChanged(kind, on);
            });
            hbox->addWidget(box);
            auto *lbl = new ClickableLabel;
            lbl->setCursor(Qt::PointingHandCursor);
            connect(lbl, &ClickableLabel::clicked, box, &QCheckBox::click);
            lbl->setText(rowText(row));
            lbl->setTextInteractionFlags(Qt::NoTextInteraction);
            hbox->addWidget(lbl);
            m_rowLabels.append(lbl);
            applyRowStyle(lbl, row.kind);
            m_rowsLayout->addLayout(hbox);
            continue;
        }
        auto *spacer = new QWidget;
        spacer->setFixedWidth(checkboxWidth);
        hbox->addWidget(spacer);
        auto *lbl = new QLabel;
        lbl->setText(rowText(row));
        lbl->setTextInteractionFlags(Qt::NoTextInteraction);
        hbox->addWidget(lbl);
        m_rowsLayout->addLayout(hbox);
        m_rowLabels.append(lbl);
        applyRowStyle(lbl, row.kind);
    }
    relayout();
}

void IssueSummaryPane::applyRowStyle(QLabel *label, Kind kind)
{
    if (!m_checked.value(kind, true)) {
        QColor dimmed = m_fg;
        dimmed.setAlpha(90);
        label->setStyleSheet(QStringLiteral("color: %1;").arg(dimmed.name(QColor::HexArgb)));
    } else {
        label->setStyleSheet(QString());
    }
}

void IssueSummaryPane::relayout()
{
    if (auto *outer = layout())
        outer->invalidate();
    adjustSize();
    if (isVisible()) {
        setGeometry(geometry().x(), geometry().y(), sizeHint().width(), sizeHint().height());
    }
    updateGeometry();
    update();
}

void IssueSummaryPane::onTimeout()
{
    hide();
    emit closeRequested();
}

#include "IssueSummaryPane.moc"