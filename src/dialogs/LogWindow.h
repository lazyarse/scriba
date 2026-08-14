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
#include <QTextEdit>

class LogWindow : public QDialog
{
    Q_OBJECT

public:
    enum Level { Info, Warning, Error };
    Q_ENUM(Level)

    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow() override;

    static void initDebugLogging();
    void addEntry(Level level, const QString &source, const QString &message);

private:
    QTextEdit *m_output;
};
