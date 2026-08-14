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
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

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
    QColor bgAlpha = background;
    bgAlpha.setAlpha(205);
    QColor hover = foreground;
    hover.setAlpha(40);
    setStyleSheet(QString(
        "#issue-summary-pane {"
        "  background-color: rgba(%1,%2,%3,%4);"
        "  border: 1px solid %5;"
        "  border-radius: 8px;"
        "}"
        "#issue-summary-pane QLabel { color: %6; background: transparent; }"
        "#issue-summary-pane QToolButton { color: %6; background: transparent;"
        "  border: none; font-size: 11pt; }"
        "#issue-summary-pane QToolButton:hover {"
        "  background-color: rgba(%7,%8,%9,%10); border-radius: 4px; }")
        .arg(bgAlpha.red()).arg(bgAlpha.green()).arg(bgAlpha.blue()).arg(bgAlpha.alpha())
        .arg(foreground.name())
        .arg(foreground.name())
        .arg(hover.red()).arg(hover.green()).arg(hover.blue()).arg(hover.alpha()));
}

void IssueSummaryPane::setRows(const QVector<Row> &rows)
{
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
            w->deleteLater();
        delete item;
    }
    for (const Row &row : m_rows) {
        auto *lbl = new QLabel;
        lbl->setText(QStringLiteral("<span style=\"color:%1;\">\u25cf</span> %2: <b>%3</b>")
                         .arg(row.color.name(), row.label.toHtmlEscaped())
                         .arg(row.count));
        lbl->setTextInteractionFlags(Qt::NoTextInteraction);
        m_rowsLayout->addWidget(lbl);
    }
    adjustSize();
}

void IssueSummaryPane::onTimeout()
{
    hide();
    emit closeRequested();
}