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
#include "EmojiDialog.h"
#include "StaticHelpers.h"
#include "prefs/Preferences.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSettings>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QApplication>
#include <QFont>

EmojiDialog::EmojiDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Emoji Picker");
    setMinimumSize(520, 450);
    resize(640, 550);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(10, 10, 10, 10);

    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Search emoji by name...");
    m_searchBox->setClearButtonEnabled(true);
    layout->addWidget(m_searchBox);

    m_list = new QListWidget();
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(36, 36));
    m_list->setGridSize(QSize(80, 68));
    m_list->setWordWrap(true);
    m_list->setSpacing(2);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list, 1);

    m_selectedLabel = new QLabel();
    m_selectedLabel->setAlignment(Qt::AlignCenter);
    m_selectedLabel->setMinimumHeight(24);
    layout->addWidget(m_selectedLabel);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Cancel)->setText("&Cancel");
    m_insertBtn = buttonBox->addButton("&Insert", QDialogButtonBox::AcceptRole);
    m_insertBtn->setEnabled(false);
    layout->addWidget(buttonBox);

    stripButtonIcons(buttonBox);

    connect(m_searchBox, &QLineEdit::textChanged, this, &EmojiDialog::filterEmoji);
    connect(m_list, &QListWidget::itemClicked, this, &EmojiDialog::onItemClicked);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        onItemClicked(item);
        if (!m_selected.isEmpty()) emit emojiChosen(m_selected);
    });
    connect(m_insertBtn, &QPushButton::clicked, this, [this]() {
        if (!m_selected.isEmpty()) emit emojiChosen(m_selected);
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_colorMode = Preferences::emojiRenderingFromString(
        QSettings().value(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString())
        == Preferences::EmojiRendering::Color;
    loadEmojiData();
    filterEmoji(QString());

    m_searchBox->setFocus();
    QApplication::processEvents();
}

QString EmojiDialog::selectedShortcode() const
{
    return m_selected;
}

void EmojiDialog::loadEmojiData()
{
    m_all = emojiCatalog();
}

void EmojiDialog::filterEmoji(const QString &text)
{
    m_list->clear();
    m_selected.clear();
    m_selectedLabel->clear();

    QString filter = text.trimmed().toLower();
    int iconSize = 36;
    int fontSize = 10;

    QColor bg = palette().window().color();
    bool darkBg = bg.lightness() < 128;

    // Empty search keeps the catalog's alphabetical grid; otherwise fuzzy-match
    // and rank by relevance (tighter subsequence matches first).
    QList<EmojiEntry> ordered;
    if (filter.isEmpty()) {
        ordered = m_all;
    } else {
        QVector<QPair<const EmojiEntry *, FuzzyScore>> scored;
        for (const EmojiEntry &entry : m_all) {
            FuzzyScore score = fuzzyMatchScore(entry.shortcode, filter);
            if (score.matched)
                scored.append({&entry, score});
        }
        std::sort(scored.begin(), scored.end(),
            [](const QPair<const EmojiEntry *, FuzzyScore> &a,
               const QPair<const EmojiEntry *, FuzzyScore> &b) {
                if (a.second.gaps != b.second.gaps)
                    return a.second.gaps < b.second.gaps;
                if (a.second.firstPos != b.second.firstPos)
                    return a.second.firstPos < b.second.firstPos;
                return a.first->shortcode < b.first->shortcode;
            });
        for (const auto &match : scored)
            ordered.append(*match.first);
    }

    for (const EmojiEntry &entry : ordered) {

        QPixmap pix(iconSize, iconSize);
        pix.fill(Qt::transparent);

        // A light disc sits behind glyph-rendered emoji on dark themes (color
        // mode with a real twemoji SVG doesn't need one). The shared renderer
        // supplies the glyph/SVG, so the disc is composited underneath.
        bool svgShown = m_colorMode && !emojiTwemojiPath(entry.unicode).isEmpty();
        if (darkBg && !svgShown) {
            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(220, 220, 220));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QRectF(2, 2, iconSize - 4, iconSize - 4));
        }
        {
            QPainter painter(&pix);
            painter.drawPixmap(0, 0, renderEmojiPixmap(entry.unicode, iconSize));
        }

        auto *item = new QListWidgetItem(QIcon(pix), entry.shortcode);
        item->setData(Qt::UserRole, entry.shortcode);
        item->setSizeHint(QSize(80, 68));
        QFont f = item->font();
        f.setPointSize(fontSize);
        item->setFont(f);
        m_list->addItem(item);
    }
}

void EmojiDialog::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    m_selected = item->data(Qt::UserRole).toString();
    m_selectedLabel->setText(QString(":%1:").arg(m_selected));
    m_insertBtn->setEnabled(true);
}
