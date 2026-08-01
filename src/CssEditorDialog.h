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
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>

class CssHighlighter;

class CssEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CssEditorDialog(const QString &title, const QString &css, const QString &defaultCss,
                             const QString &themeCss, QWidget *parent = nullptr);
    QString css() const;

private:
    void applyFontPreset();

    QString m_defaultCss;
    QPlainTextEdit *m_editor;
    QComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;
};

