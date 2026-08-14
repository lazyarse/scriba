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

#include <QObject>
#include <QString>

// Bridge between the preview page's JavaScript and C++. Registered in a
// QWebChannel as "scriba"; the preview's click handler calls openLink() /
// editChart() through it. Replaces the old fragment-based link routing
// (#scriba-open:/#scriba-edit: URLs watched via QWebEnginePage::urlChanged),
// which was racy: a lost fragment-clearing replaceState left a stale URL so
// a repeated click became a no-op replaceState and was silently dropped.
class PreviewBridge : public QObject
{
    Q_OBJECT

public:
    explicit PreviewBridge(QObject *parent = nullptr);

    Q_INVOKABLE void openLink(const QString &href);
    Q_INVOKABLE void editChart(const QString &kind, int line, int index, const QString &tex);

signals:
    void linkRequested(const QString &href);
    void chartEditRequested(const QString &kind, int line, int index, const QString &tex);
};