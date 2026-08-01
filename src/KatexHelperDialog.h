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
#include <QColor>

class QWebEngineView;
class QPlainTextEdit;
class QRadioButton;
class QTimer;

class KatexHelperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KatexHelperDialog(const QString &themeCss = QString(), QWidget *parent = nullptr);
    QString generatedLatex() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QWidget *createSymbolPalette(QWidget *parent);
    QWidget *createCheatSheet(QWidget *parent);
    void insertAtCursor(const QString &text);

    QWebEngineView *m_preview;
    QPlainTextEdit *m_input;
    QRadioButton *m_inlineRadio;
    QRadioButton *m_blockRadio;
    QTimer *m_previewTimer;
    QColor m_themeBg;
    QColor m_themeTxt;
};

