#ifndef EMOJIDIALOG_H
#define EMOJIDIALOG_H

#include <QDialog>
#include <QString>
#include <QList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

class EmojiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmojiDialog(QWidget *parent = nullptr);
    QString selectedShortcode() const;

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
    QList<EmojiEntry> m_all;
    QString m_selected;
    bool m_colorMode = false;
};

#endif
