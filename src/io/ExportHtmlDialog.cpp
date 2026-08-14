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
#include "ExportHtmlDialog.h"
#include "css/CssConfig.h"
#include "css/CssLoader.h"
#include "StaticHelpers.h"
#include "prefs/Preferences.h"
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

ExportHtmlDialog::ExportHtmlDialog(CssConfig *config, CssLoader *loader,
                                   const QString &defaultFilePath,
                                   QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_loader(loader)
    , m_defaultFilePath(defaultFilePath)
{
    setWindowTitle("Export as HTML");
    resize(400, 200);
    setupUi();
}

QString ExportHtmlDialog::selectedThemePath() const
{
    if (!m_themeCombo || m_themeCombo->currentIndex() < 0)
        return {};
    return m_themeCombo->currentData().toString();
}

ScriptHandling ExportHtmlDialog::selectedScriptHandling() const
{
    if (!m_scriptCombo || m_scriptCombo->currentIndex() < 0)
        return ScriptHandling::Strip;
    return static_cast<ScriptHandling>(m_scriptCombo->currentData().toInt());
}

void ExportHtmlDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *themeLabel = new QLabel("Theme:", this);
    layout->addWidget(themeLabel);

    m_themeCombo = new QComboBox(this);
    layout->addWidget(m_themeCombo);

    // Populate themes from the user's stylesheet list
    QString activeTheme = m_config->activeStylesheet();
    int activeIndex = 0;

    QStringList themes = m_config->stylesheets();
    for (int i = 0; i < themes.size(); ++i) {
        QString path = themes[i];
        // Display name: strip to just the filename
        QString name = path;
        if (name.startsWith(":/"))
            name = name.mid(2);
        if (name.endsWith(".css"))
            name.chop(4);
        // Strip themes/ prefix for cleaner display
        if (name.startsWith("themes/"))
            name = name.mid(7);
        // If still a full path (user-imported), take just the filename
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0)
            name = name.mid(lastSlash + 1);
        name.replace("-", " ");

        m_themeCombo->addItem(name, path);
        if (path == activeTheme)
            activeIndex = i;
    }

    m_themeCombo->setCurrentIndex(activeIndex);

    auto *scriptLabel = new QLabel("External scripts:", this);
    layout->addWidget(scriptLabel);

    m_scriptCombo = new QComboBox(this);
    m_scriptCombo->addItem("Strip (recommended)", static_cast<int>(ScriptHandling::Strip));
    m_scriptCombo->addItem("Embed inline", static_cast<int>(ScriptHandling::EmbedExternal));
    bool stripDefault = QSettings().value(Preferences::StripExportScripts, true).toBool();
    m_scriptCombo->setCurrentIndex(stripDefault ? 0 : 1);
    layout->addWidget(m_scriptCombo);

    layout->addStretch();

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);
}
