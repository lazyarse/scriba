#include "PreferencesDialog.h"
#include "Editor.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>
#include <QGroupBox>
#include <QColorDialog>
#include <QLabel>

PreferencesDialog::PreferencesDialog(CssManager *cssManager, Editor *editor, QWidget *parent)
    : QDialog(parent)
    , m_cssManager(cssManager)
    , m_editor(editor)
{
    setupUi();
    setWindowTitle("Preferences");
    resize(500, 500);
}

void PreferencesDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    /* --- Editor Font Group --- */
    QGroupBox *fontGroup = new QGroupBox("Editor Font");
    QGridLayout *fontLayout = new QGridLayout(fontGroup);

    fontLayout->addWidget(new QLabel("Font:"), 0, 0);
    m_fontCombo = new QFontComboBox();
    fontLayout->addWidget(m_fontCombo, 0, 1);

    fontLayout->addWidget(new QLabel("Size:"), 0, 2);
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(8, 72);
    fontLayout->addWidget(m_fontSizeSpin, 0, 3);

    fontLayout->addWidget(new QLabel("Color:"), 1, 0);
    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedWidth(80);
    fontLayout->addWidget(m_colorBtn, 1, 1);

    /* Load current settings */
    QSettings settings;
    QString family = settings.value(Preferences::EditorFont, "Monospace").toString();
    int size = settings.value(Preferences::EditorFontSize, 14).toInt();
    m_fontColor = QColor(settings.value(Preferences::EditorFontColor, "#333333").toString());

    QFont currentFont(family, size);
    m_fontCombo->setCurrentFont(currentFont);
    m_fontSizeSpin->setValue(size);

    /* Color button styling */
    m_colorBtn->setStyleSheet(QString("background-color: %1;").arg(m_fontColor.name()));
    connect(m_colorBtn, &QPushButton::clicked, this, &PreferencesDialog::pickFontColor);

    mainLayout->addWidget(fontGroup);

    /* --- Behavior Group --- */
    QGroupBox *behaviorGroup = new QGroupBox("Behavior");
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);

    m_reopenCheck = new QCheckBox("Re-open last edited file on startup");
    m_reopenCheck->setChecked(settings.value(Preferences::ReopenLastFile, true).toBool());
    behaviorLayout->addWidget(m_reopenCheck);

    m_syncCheck = new QCheckBox("Sync editor and preview scrolling");
    m_syncCheck->setChecked(settings.value(Preferences::SyncScroll, true).toBool());
    behaviorLayout->addWidget(m_syncCheck);

    mainLayout->addWidget(behaviorGroup);

    /* --- Stylesheets Group --- */
    QGroupBox *cssGroup = new QGroupBox("Stylesheets");
    QVBoxLayout *cssLayout = new QVBoxLayout(cssGroup);

    QHBoxLayout *listLayout = new QHBoxLayout();
    m_listWidget = new QListWidget();
    m_addButton = new QPushButton("Add");
    m_removeButton = new QPushButton("Remove");

    QVBoxLayout *btnLayout = new QVBoxLayout();
    btnLayout->addWidget(m_addButton);
    btnLayout->addWidget(m_removeButton);
    btnLayout->addStretch();

    listLayout->addWidget(m_listWidget);
    listLayout->addLayout(btnLayout);
    cssLayout->addLayout(listLayout);

    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addStylesheet);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeStylesheet);
    connect(m_listWidget, &QListWidget::itemChanged, this, &PreferencesDialog::onItemChanged);

    populateStylesheetList();

    mainLayout->addWidget(cssGroup);

    /* --- Dialog Buttons --- */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::saveFontSettings);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PreferencesDialog::populateStylesheetList()
{
    m_listWidget->clear();
    QString active = m_cssManager->activeStylesheet();

    for (const QString &path : m_cssManager->stylesheets()) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(path == active ? Qt::Checked : Qt::Unchecked);
        m_listWidget->addItem(item);
    }
}

void PreferencesDialog::addStylesheet()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select CSS Files", QString(), "CSS Files (*.css)");
    if (files.isEmpty()) return;

    QStringList existing = m_cssManager->stylesheets();
    for (const QString &file : files) {
        if (!existing.contains(file))
            existing.append(file);
    }
    m_cssManager->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::removeStylesheet()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    QStringList existing = m_cssManager->stylesheets();
    existing.removeAll(path);

    if (path == m_cssManager->activeStylesheet())
        m_cssManager->setActiveStylesheet(QString());

    m_cssManager->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::onItemChanged(QListWidgetItem *item)
{
    if (m_updatingCheckState) return;
    m_updatingCheckState = true;

    if (item->checkState() == Qt::Checked) {
        for (int i = 0; i < m_listWidget->count(); i++) {
            QListWidgetItem *other = m_listWidget->item(i);
            if (other != item)
                other->setCheckState(Qt::Unchecked);
        }
        m_cssManager->setActiveStylesheet(item->data(Qt::UserRole).toString());
        emit stylesheetChanged();
    } else {
        bool anyChecked = false;
        for (int i = 0; i < m_listWidget->count(); i++) {
            if (m_listWidget->item(i)->checkState() == Qt::Checked) {
                anyChecked = true;
                break;
            }
        }
        if (!anyChecked) {
            m_cssManager->setActiveStylesheet(QString());
            emit stylesheetChanged();
        }
    }

    m_updatingCheckState = false;
}

void PreferencesDialog::pickFontColor()
{
    QColor color = QColorDialog::getColor(m_fontColor, this, "Select Font Color");
    if (color.isValid()) {
        m_fontColor = color;
        m_colorBtn->setStyleSheet(QString("background-color: %1;").arg(m_fontColor.name()));
    }
}

void PreferencesDialog::saveFontSettings()
{
    QSettings settings;
    settings.setValue(Preferences::EditorFont, m_fontCombo->currentFont().family());
    settings.setValue(Preferences::EditorFontSize, m_fontSizeSpin->value());
    settings.setValue(Preferences::EditorFontColor, m_fontColor.name());
    settings.setValue(Preferences::ReopenLastFile, m_reopenCheck->isChecked());
    settings.setValue(Preferences::SyncScroll, m_syncCheck->isChecked());
}
