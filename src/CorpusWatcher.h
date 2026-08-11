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

#include <QHash>
#include <QObject>
#include <QStringList>

class QTimer;
class QFileSystemWatcher;

// Watches a corpus's document files for external edits/renames/deletes.
// Detection is rescan-plus-diff (not per-file path events): after a debounce
// the known files' content hashes are compared against their last-seen
// hashes; a vanished file whose content hash matches a fresh (previously
// unknown) file in a watched directory is reported as a rename, with the
// content hash pairing as cheap disambiguation for swaps.
class CorpusWatcher : public QObject
{
    Q_OBJECT
public:
    explicit CorpusWatcher(QObject *parent = nullptr);

    // Watches each existing absolute path plus the distinct parent directories.
    void setMonitoredFiles(const QStringList &filePaths);
    void clear();

signals:
    void edited(const QString &path);
    void renamed(const QString &from, const QString &to);
    void deleted(const QString &path);

private:
    void onChanged();
    void finishDebounce();
    void diffAndEmit();

    QTimer *m_debounce = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QStringList m_files;                 // known absolute doc paths
    QHash<QString, QByteArray> m_hashes; // last-seen content hashes
};
