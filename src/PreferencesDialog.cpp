#include "PreferencesDialog.h"
#include "Editor.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
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

    fontLayout->addWidget(new QLabel("Background:"), 1, 2);
    m_bgColorBtn = new QPushButton();
    m_bgColorBtn->setFixedWidth(80);
    fontLayout->addWidget(m_bgColorBtn, 1, 3);

    /* Load current settings */
    QSettings settings;
    QString family = settings.value(Preferences::EditorFont, "Monospace").toString();
    int size = settings.value(Preferences::EditorFontSize, 14).toInt();
    m_fontColor = QColor(settings.value(Preferences::EditorFontColor, "#333333").toString());
    m_bgColor = QColor(settings.value(Preferences::EditorBgColor, "#ffffff").toString());

    QFont currentFont(family, size);
    m_fontCombo->setCurrentFont(currentFont);
    m_fontSizeSpin->setValue(size);

    /* Color button styling */
    m_colorBtn->setStyleSheet(QString("background-color: %1;").arg(m_fontColor.name()));
    connect(m_colorBtn, &QPushButton::clicked, this, &PreferencesDialog::pickFontColor);
    m_bgColorBtn->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    connect(m_bgColorBtn, &QPushButton::clicked, this, &PreferencesDialog::pickBgColor);

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

    /* --- CSS Group --- */
    QGroupBox *cssGroup = new QGroupBox("Stylesheets");
    QVBoxLayout *cssLayout = new QVBoxLayout(cssGroup);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    QLabel *dirLabel = new QLabel("CSS Directory:");
    QLineEdit *dirEdit = new QLineEdit(m_cssManager->cssDirectory());
    QPushButton *browseBtn = new QPushButton("Browse...");
    dirLayout->addWidget(dirLabel);
    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(browseBtn);
    cssLayout->addLayout(dirLayout);

    connect(browseBtn, &QPushButton::clicked, this, &PreferencesDialog::selectDirectory);

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

    m_previewLabel = new QLabel();
    cssLayout->addWidget(m_previewLabel);

    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addCssFile);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeCssFile);

    mainLayout->addWidget(cssGroup);

    /* --- Dialog Buttons --- */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::saveFontSettings);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (const QString &file : m_cssManager->enabledFiles()) {
        m_listWidget->addItem(file);
    }
}

void PreferencesDialog::pickFontColor()
{
    QColor color = QColorDialog::getColor(m_fontColor, this, "Select Font Color");
    if (color.isValid()) {
        m_fontColor = color;
        m_colorBtn->setStyleSheet(QString("background-color: %1;").arg(m_fontColor.name()));
    }
}

void PreferencesDialog::pickBgColor()
{
    QColor color = QColorDialog::getColor(m_bgColor, this, "Select Background Color");
    if (color.isValid()) {
        m_bgColor = color;
        m_bgColorBtn->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    }
}

void PreferencesDialog::saveFontSettings()
{
    QSettings settings;
    settings.setValue(Preferences::EditorFont, m_fontCombo->currentFont().family());
    settings.setValue(Preferences::EditorFontSize, m_fontSizeSpin->value());
    settings.setValue(Preferences::EditorFontColor, m_fontColor.name());
    settings.setValue(Preferences::EditorBgColor, m_bgColor.name());
    settings.setValue(Preferences::ReopenLastFile, m_reopenCheck->isChecked());
    settings.setValue(Preferences::SyncScroll, m_syncCheck->isChecked());
}

void PreferencesDialog::selectDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select CSS Directory", m_cssManager->cssDirectory());
    if (!dir.isEmpty()) {
        m_cssManager->setCssDirectory(dir);
        m_listWidget->clear();
        for (const QString &file : m_cssManager->enabledFiles()) {
            m_listWidget->addItem(file);
        }
    }
}

void PreferencesDialog::addCssFile()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select CSS Files", m_cssManager->cssDirectory(), "CSS Files (*.css)");
    QStringList enabled = m_cssManager->enabledFiles();
    for (const QString &file : files) {
        QFileInfo info(file);
        if (!enabled.contains(info.fileName())) {
            enabled.append(info.fileName());
        }
    }
    m_cssManager->setEnabledFiles(enabled);
    m_listWidget->clear();
    for (const QString &f : enabled) {
        m_listWidget->addItem(f);
    }
}

void PreferencesDialog::removeCssFile()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QStringList enabled = m_cssManager->enabledFiles();
    enabled.removeAll(item->text());
    m_cssManager->setEnabledFiles(enabled);
    delete m_listWidget->takeItem(m_listWidget->row(item));
}
