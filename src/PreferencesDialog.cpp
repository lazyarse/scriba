#include "PreferencesDialog.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssEditorDialog.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QIcon>
#include <QFile>

PreferencesDialog::PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_loader(loader)
{
    setupUi();
    setWindowTitle("Preferences");
    resize(500, 400);
}

void PreferencesDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    /* --- Behavior Group --- */
    QGroupBox *behaviorGroup = new QGroupBox("Behavior");
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);

    QSettings settings;
    m_reopenCheck = new QCheckBox("Re-open last edited file on startup");
    m_reopenCheck->setChecked(settings.value(Preferences::ReopenLastFile, true).toBool());
    behaviorLayout->addWidget(m_reopenCheck);

    m_syncCheck = new QCheckBox("Sync editor and preview scrolling");
    m_syncCheck->setChecked(settings.value(Preferences::SyncScroll, true).toBool());
    behaviorLayout->addWidget(m_syncCheck);

    m_stripeCheck = new QCheckBox("Alternating table row colors");
    m_stripeCheck->setChecked(settings.value(Preferences::TableStriping, true).toBool());
    behaviorLayout->addWidget(m_stripeCheck);

    QHBoxLayout *posLayout = new QHBoxLayout();
    posLayout->addWidget(new QLabel("Editor position:"));
    m_editorPositionCombo = new QComboBox();
    m_editorPositionCombo->addItems({"Left", "Right"});
    m_editorPositionCombo->setCurrentIndex(settings.value(Preferences::EditorOnLeft, true).toBool() ? 0 : 1);
    posLayout->addWidget(m_editorPositionCombo);
    posLayout->addStretch();
    behaviorLayout->addLayout(posLayout);

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

    m_editEditorBtn = new QPushButton("Edit Editor Base CSS...");
    m_editPreviewBtn = new QPushButton("Edit Preview Base CSS...");

    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addStylesheet);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeStylesheet);
    connect(m_editEditorBtn, &QPushButton::clicked, this, &PreferencesDialog::editEditorBaseCss);
    connect(m_editPreviewBtn, &QPushButton::clicked, this, &PreferencesDialog::editPreviewBaseCss);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, &PreferencesDialog::onCurrentItemChanged);

    populateStylesheetList();

    QHBoxLayout *baseBtnLayout = new QHBoxLayout();
    baseBtnLayout->addWidget(m_editEditorBtn);
    baseBtnLayout->addWidget(m_editPreviewBtn);
    baseBtnLayout->addStretch();

    mainLayout->addLayout(baseBtnLayout);
    mainLayout->addWidget(cssGroup);

    /* --- Dialog Buttons --- */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        QSettings settings;
        settings.setValue(Preferences::ReopenLastFile, m_reopenCheck->isChecked());
        settings.setValue(Preferences::SyncScroll, m_syncCheck->isChecked());
        settings.setValue(Preferences::EditorOnLeft, m_editorPositionCombo->currentIndex() == 0);
        settings.setValue(Preferences::TableStriping, m_stripeCheck->isChecked());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PreferencesDialog::populateStylesheetList()
{
    m_listWidget->clear();
    QString active = m_config->activeStylesheet();

    for (const QString &path : m_config->stylesheets()) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_listWidget->addItem(item);
        if (path == active)
            m_listWidget->setCurrentItem(item);
    }
}

void PreferencesDialog::addStylesheet()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select CSS Files", QString(), "CSS Files (*.css)");
    if (files.isEmpty()) return;

    QStringList existing = m_config->stylesheets();
    for (const QString &file : files) {
        if (!existing.contains(file))
            existing.append(file);
    }
    m_config->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::removeStylesheet()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    QStringList existing = m_config->stylesheets();
    existing.removeAll(path);

    if (path == m_config->activeStylesheet())
        m_config->setActiveStylesheet(QString());

    m_config->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

static QString loadResourceCss(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

void PreferencesDialog::editEditorBaseCss()
{
    CssEditorDialog dlg("Edit Editor Base CSS", m_loader->editorBaseCss(),
        loadResourceCss(":/editor-base.css"), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_loader->setEditorBaseCss(dlg.css());
        emit stylesheetChanged();
    }
}

void PreferencesDialog::editPreviewBaseCss()
{
    CssEditorDialog dlg("Edit Preview Base CSS", m_loader->previewBaseCss(),
        loadResourceCss(":/preview-base.css"), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_loader->setPreviewBaseCss(dlg.css());
        emit stylesheetChanged();
    }
}

void PreferencesDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) return;
    m_config->setActiveStylesheet(current->data(Qt::UserRole).toString());
    emit stylesheetChanged();
}
