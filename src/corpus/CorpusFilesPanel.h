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

#include <QWidget>
#include <QTemporaryDir>

#include <memory>

class QTreeView;
class QFileSystemModel;
class QSortFilterProxyModel;

// Sidecar listing of every file in the corpus root directory (recursive).
// QFileSystemModel's built-in watcher refreshes the listing live; the proxy
// excludes the corpus's own .scriba file and keeps directories above files.
// The panel knows nothing about tabs: it emits paths, MainWindow decides.
class CorpusFilesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CorpusFilesPanel(QWidget *parent = nullptr);

    // Roots the tree at the corpus directory. An empty dir clears the panel.
    void setRootDir(const QString &dir);
    // Absolute path to hide from the listing (the corpus .scriba file).
    void setExcludedPath(const QString &absPath);
    void clear();

signals:
    void fileActivated(const QString &absPath);        // double-click / Enter on a file
    void insertLinkRequested(const QString &absPath);  // context menu
    void copyPathRequested(const QString &absPath);    // context menu
    void openExternalRequested(const QString &absPath);// context menu

private:
    void onIndexActivated(const QModelIndex &proxyIndex);
    void showContextMenu(const QPoint &pos);

    QTreeView *m_tree = nullptr;
    QFileSystemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QString m_rootDir;
    QString m_excludedPath;
    QString m_lastEmitted;              // dedupe: Qt emits both `activated` and
    qint64 m_lastEmittedAt = 0;         // `doubleClicked` for one double-click
    std::unique_ptr<QTemporaryDir> m_emptyRoot;
};