#include "EmojiDialog.h"
#include "Preferences.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSettings>
#include <QFile>
#include <QRegularExpression>
#include <QSvgRenderer>
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
    auto *insertBtn = buttonBox->addButton("Insert", QDialogButtonBox::AcceptRole);
    insertBtn->setEnabled(false);
    layout->addWidget(buttonBox);

    for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());

    connect(m_searchBox, &QLineEdit::textChanged, this, &EmojiDialog::filterEmoji);
    connect(m_list, &QListWidget::itemClicked, this, &EmojiDialog::onItemClicked);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_colorMode = QSettings().value(Preferences::EmojiMode, "bw").toString() == "color";
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
    QFile file(":/emoji.js");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(file.readAll());

    QRegularExpression re(R"('([^']+)'\s*:\s*'([^']+)')");
    auto it = re.globalMatch(content);

    while (it.hasNext()) {
        auto match = it.next();
        EmojiEntry entry;
        entry.shortcode = match.captured(1);
        entry.unicode = match.captured(2);
        entry.codePoint = unicodeToCodePoint(entry.unicode);
        resolveSvgPath(entry);
        m_all.append(entry);
    }
}

QString EmojiDialog::unicodeToCodePoint(const QString &unicode)
{
    QStringList parts;
    auto ucs4 = unicode.toUcs4();
    for (uint cp : ucs4)
        parts.append(QString::number(cp, 16));
    return parts.join('-');
}

QString EmojiDialog::stripFe0f(const QString &codePoint)
{
    QStringList parts = codePoint.split('-');
    parts.removeAll("fe0f");
    return parts.join('-');
}

bool EmojiDialog::resolveSvgPath(EmojiEntry &entry) const
{
    QString path = QString(":/twemoji/svg/%1.svg").arg(entry.codePoint);
    if (QFile::exists(path))
        return true;

    QString stripped = stripFe0f(entry.codePoint);
    if (stripped != entry.codePoint) {
        path = QString(":/twemoji/svg/%1.svg").arg(stripped);
        if (QFile::exists(path)) {
            entry.codePoint = stripped;
            return true;
        }
    }

    return false;
}

void EmojiDialog::filterEmoji(const QString &text)
{
    m_list->clear();
    m_selected.clear();
    m_selectedLabel->clear();

    QString filter = text.trimmed().toLower();
    int iconSize = 36;
    int fontSize = 10;

    for (const auto &entry : m_all) {
        if (!filter.isEmpty() && !entry.shortcode.contains(filter))
            continue;

        QPixmap pix(iconSize, iconSize);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);

        if (m_colorMode && QFile::exists(QString(":/twemoji/svg/%1.svg").arg(entry.codePoint))) {
            QSvgRenderer renderer(QString(":/twemoji/svg/%1.svg").arg(entry.codePoint));
            renderer.render(&painter, QRectF(0, 0, iconSize, iconSize));
        } else {
            QFont f = painter.font();
            f.setPixelSize(iconSize - 4);
            painter.setFont(f);
            painter.setPen(palette().windowText().color());
            painter.drawText(QRect(0, 0, iconSize, iconSize), Qt::AlignCenter, entry.unicode);
        }
        painter.end();

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
}
