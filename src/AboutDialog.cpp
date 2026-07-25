#include "AboutDialog.h"

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
        "A configurable, no-nonsense, split-screen, off-line "
        "Markdown editor with a pretentious Latin name. "
        "An opinionated restricted feature-set: useful; no bloat, "
        "and designed to actually do what it should without plugin hell."
    );
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(descLabel);

    mainLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttonBox);
}
