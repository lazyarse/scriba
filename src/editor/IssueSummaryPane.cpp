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
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
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
    QColor hover = foreground;
    hover.setAlpha(40);
    setStyleSheet(QString(
        "#issue-summary-pane QLabel { color: %1; background: transparent; }"
        "#issue-summary-pane QToolButton { color: %1; background: transparent;"
        "  border: none; font-size: 11pt; }"
        "#issue-summary-pane QToolButton:hover {"
        "  background-color: rgba(%2,%3,%4,%5); border-radius: 4px; }")
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
    // Structural identity: same row count with the same kinds and labels in
    // order. Counts don't affect structure, so a count-only update reuses the
    // existing QLabels in place instead of deleting and recreating every row.
    bool sameStructure = rows.size() == m_rows.size();
    if (sameStructure) {
        for (int i = 0; i < rows.size(); ++i) {
            if (rows.at(i).kind != m_rows.at(i).kind
                || rows.at(i).label != m_rows.at(i).label) {
                sameStructure = false;
                break;
            }
        }
    }
    if (sameStructure) {
        for (int i = 0; i < m_rowLabels.size(); ++i)
            m_rowLabels.at(i)->setText(rowText(rows.at(i)));
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
        if (QWidget *w = item->widget())
            delete w;
        delete item;
    }
    m_rowLabels.clear();
    for (const Row &row : m_rows) {
        auto *lbl = new QLabel;
        lbl->setText(rowText(row));
        lbl->setTextInteractionFlags(Qt::NoTextInteraction);
        m_rowsLayout->addWidget(lbl);
        m_rowLabels.append(lbl);
    }
    relayout();
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