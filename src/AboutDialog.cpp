#include "AboutDialog.h"
#include "StaticHelpers.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPushButton>

#ifndef SCRIBA_VERSION
#define SCRIBA_VERSION "dev"
#endif

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("About Scriba");
    setFixedSize(420, 270);
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(24, 24, 24, 16);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(16);

    auto *iconLabel = new QLabel();
    QPixmap pix(":/icons/scriba.svg");
    if (!pix.isNull())
        iconLabel->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(72, 72);
    iconLabel->setAlignment(Qt::AlignCenter);
    topRow->addWidget(iconLabel);

    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(4);

    auto *titleLabel = new QLabel("Scriba");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    textCol->addWidget(titleLabel);

    auto *urlLabel = new QLabel("<a href=\"https://www.github.com/lazyarse/scriba\">github.com/lazyarse/scriba</a>");
    urlLabel->setOpenExternalLinks(true);
    urlLabel->setTextFormat(Qt::RichText);
    textCol->addWidget(urlLabel);

    auto *versionLabel = new QLabel(QString("Version %1").arg(SCRIBA_VERSION));
    auto verPalette = versionLabel->palette();
    verPalette.setColor(versionLabel->foregroundRole(), verPalette.color(QPalette::PlaceholderText));
    versionLabel->setPalette(verPalette);
    textCol->addWidget(versionLabel);

    textCol->addStretch();
    topRow->addLayout(textCol);
    topRow->addStretch();

    mainLayout->addLayout(topRow);

    auto *descLabel = new QLabel(
        "A privacy-first, no-nonsense, configurable Markdown editor for "
        "technical people. Designed to *actually* do what you want without "
        "plugin hell. No node / react / angular / bloat, just a binary that "
        "sits on your computer and doesn't call outside."
    );
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(descLabel);

    mainLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    buttonBox->button(QDialogButtonBox::Close)->setText(tr("&Close"));
    stripButtonIcons(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttonBox);
}
