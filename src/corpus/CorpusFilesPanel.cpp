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
#include "CorpusFilesPanel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTemporaryDir>
#include <QTreeView>
#include <QVBoxLayout>

class CorpusFilesProxy : public QSortFilterProxyModel
{
public:
    explicit CorpusFilesProxy(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setExcludedPath(const QString &path)
    {
        m_excluded = path;
        beginFilterChange();
        endFilterChange();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (m_excluded.isEmpty())
            return true;
        const auto *fs = static_cast<QFileSystemModel *>(sourceModel());
        const QModelIndex src = fs->index(row, 0, parent);
        return QFileInfo(fs->filePath(src)).absoluteFilePath()
            != QFileInfo(m_excluded).absoluteFilePath();
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        auto *fs = static_cast<QFileSystemModel *>(sourceModel());
        const bool lDir = fs->isDir(left);
        const bool rDir = fs->isDir(right);
        if (lDir != rDir)
            return lDir;                 // directories always above files
        return QString::compare(left.data().toString(),
                                right.data().toString(), Qt::CaseInsensitive) < 0;
    }

private:
    QString m_excluded;
};

CorpusFilesPanel::CorpusFilesPanel(QWidget *parent)
    : QWidget(parent)
{
    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new QFileSystemModel(this);
    // Default filter (AllEntries | NoDotAndDotDot | AllDirs) excludes hidden
    // dotfiles; the model's internal QFileSystemWatcher drives live updates.
    m_model->setReadOnly(true);

    m_proxy = new CorpusFilesProxy(this);
    m_proxy->setSourceModel(m_model);
    m_tree->setModel(m_proxy);

    connect(m_tree, &QTreeView::doubleClicked, this,
            [this](const QModelIndex &idx) { onIndexActivated(idx); });
    connect(m_tree, &QTreeView::activated, this,
            [this](const QModelIndex &idx) { onIndexActivated(idx); });
    connect(m_tree, &QTreeView::customContextMenuRequested, this,
            &CorpusFilesPanel::showContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);
}

void CorpusFilesPanel::setRootDir(const QString &dir)
{
    if (dir == m_rootDir)
        return;
    m_rootDir = dir;
    if (dir.isEmpty()) {
        m_tree->setRootIndex(QModelIndex());
        return;
    }
    const QModelIndex root = m_model->setRootPath(dir);
    m_tree->setRootIndex(m_proxy->mapFromSource(root));
}

void CorpusFilesPanel::setExcludedPath(const QString &absPath)
{
    m_excludedPath = absPath;
    static_cast<CorpusFilesProxy *>(m_proxy)->setExcludedPath(absPath);
}

void CorpusFilesPanel::clear()
{
    m_rootDir.clear();
    // Root the model at a real but empty directory so the listing resolves to
    // zero rows; QFileSystemModel cannot be un-rooted (an empty setRootPath is
    // a no-op and a nonexistent path leaves stale rows behind).
    if (!m_emptyRoot)
        m_emptyRoot = std::make_unique<QTemporaryDir>();
    const QString empty = m_emptyRoot->path();
    m_model->setRootPath(empty);
    m_tree->setRootIndex(m_proxy->mapFromSource(m_model->index(empty)));
}

void CorpusFilesPanel::onIndexActivated(const QModelIndex &proxyIndex)
{
    const QModelIndex src = m_proxy->mapToSource(proxyIndex);
    if (!src.isValid() || m_model->isDir(src))
        return;                  // QTreeView expands directories itself
    const QString path = m_model->filePath(src);
    // Qt emits `activated` (first click of the pair) and `doubleClicked` for a
    // single double-click; act on the path once.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (path == m_lastEmitted && now - m_lastEmittedAt < 500)
        return;
    m_lastEmitted = path;
    m_lastEmittedAt = now;
    emit fileActivated(path);
}

void CorpusFilesPanel::showContextMenu(const QPoint &pos)
{
    const QModelIndex src = m_proxy->mapToSource(m_tree->indexAt(pos));
    if (!src.isValid() || m_model->isDir(src))
        return;
    const QString path = m_model->filePath(src);

    QMenu menu(this);
    QAction *insert = menu.addAction(tr("Insert Link at Cursor"));
    QAction *copy   = menu.addAction(tr("Copy Relative Path"));
    QAction *open   = menu.addAction(tr("Open with System"));
    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == insert)
        emit insertLinkRequested(path);
    else if (chosen == copy)
        emit copyPathRequested(path);
    else if (chosen == open)
        emit openExternalRequested(path);
}