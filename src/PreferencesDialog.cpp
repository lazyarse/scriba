#include "PreferencesDialog.h"
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

PreferencesDialog::PreferencesDialog(CssManager *cssManager, QWidget *parent)
    : QDialog(parent)
    , m_cssManager(cssManager)
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

    /* --- Print Stylesheets Group --- */
    QGroupBox *printGroup = new QGroupBox("Print Stylesheets");
    QVBoxLayout *printLayout = new QVBoxLayout(printGroup);

    QHBoxLayout *printListLayout = new QHBoxLayout();
    m_printListWidget = new QListWidget();
    m_printAddBtn = new QPushButton("Add");
    m_printRemoveBtn = new QPushButton("Remove");

    QVBoxLayout *printBtnLayout = new QVBoxLayout();
    printBtnLayout->addWidget(m_printAddBtn);
    printBtnLayout->addWidget(m_printRemoveBtn);
    printBtnLayout->addStretch();

    printListLayout->addWidget(m_printListWidget);
    printListLayout->addLayout(printBtnLayout);
    printLayout->addLayout(printListLayout);

    connect(m_printAddBtn, &QPushButton::clicked, this, &PreferencesDialog::addPrintStylesheet);
    connect(m_printRemoveBtn, &QPushButton::clicked, this, &PreferencesDialog::removePrintStylesheet);
    connect(m_printListWidget, &QListWidget::currentItemChanged, this, &PreferencesDialog::onPrintCurrentItemChanged);

    populatePrintStylesheetList();

    mainLayout->addWidget(printGroup);

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
        accept();
    });
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
        m_listWidget->addItem(item);
        if (path == active)
            m_listWidget->setCurrentItem(item);
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

void PreferencesDialog::populatePrintStylesheetList()
{
    m_printListWidget->clear();
    QString active = m_cssManager->activePrintStylesheet();

    for (const QString &path : m_cssManager->printStylesheets()) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_printListWidget->addItem(item);
        if (path == active)
            m_printListWidget->setCurrentItem(item);
    }
}

void PreferencesDialog::addPrintStylesheet()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Print CSS Files", QString(), "CSS Files (*.css)");
    if (files.isEmpty()) return;

    QStringList existing = m_cssManager->printStylesheets();
    for (const QString &file : files) {
        if (!existing.contains(file))
            existing.append(file);
    }
    m_cssManager->setPrintStylesheets(existing);
    populatePrintStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::removePrintStylesheet()
{
    QListWidgetItem *item = m_printListWidget->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    QStringList existing = m_cssManager->printStylesheets();
    existing.removeAll(path);

    if (path == m_cssManager->activePrintStylesheet())
        m_cssManager->setActivePrintStylesheet(QString());

    m_cssManager->setPrintStylesheets(existing);
    populatePrintStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::onPrintCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) return;
    m_cssManager->setActivePrintStylesheet(current->data(Qt::UserRole).toString());
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
    CssEditorDialog dlg("Edit Editor Base CSS", m_cssManager->editorBaseCss(),
        loadResourceCss(":/editor-base.css"), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_cssManager->setEditorBaseCss(dlg.css());
        emit stylesheetChanged();
    }
}

void PreferencesDialog::editPreviewBaseCss()
{
    CssEditorDialog dlg("Edit Preview Base CSS", m_cssManager->previewBaseCss(),
        loadResourceCss(":/preview-base.css"), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_cssManager->setPreviewBaseCss(dlg.css());
        emit stylesheetChanged();
    }
}

void PreferencesDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) return;
    m_cssManager->setActiveStylesheet(current->data(Qt::UserRole).toString());
    emit stylesheetChanged();
}
