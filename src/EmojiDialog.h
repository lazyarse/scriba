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

#include <QDialog>
#include <QString>
#include <QList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

class EmojiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmojiDialog(QWidget *parent = nullptr);
    QString selectedShortcode() const;

signals:
    void emojiChosen(const QString &shortcode);

private slots:
    void filterEmoji(const QString &text);
    void onItemClicked(QListWidgetItem *item);

private:
    struct EmojiEntry {
        QString shortcode;
        QString unicode;
        QString codePoint;
    };

    void loadEmojiData();
    void rebuildGrid();
    static QString unicodeToCodePoint(const QString &unicode);
    static QString stripFe0f(const QString &codePoint);
    bool resolveSvgPath(EmojiEntry &entry) const;

    QLineEdit *m_searchBox;
    QListWidget *m_list;
    QLabel *m_selectedLabel;
    QPushButton *m_insertBtn = nullptr;
    QList<EmojiEntry> m_all;
    QString m_selected;
    bool m_colorMode = false;
};

