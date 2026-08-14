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
#include "CorpusWatcher.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSet>
#include <QTimer>

#include "StaticHelpers.h"

static QByteArray contentHash(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

CorpusWatcher::CorpusWatcher(QObject *parent)
    : QObject(parent)
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(Debounce::CorpusWatch);
    connect(m_debounce, &QTimer::timeout, this, &CorpusWatcher::finishDebounce);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &CorpusWatcher::onChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &CorpusWatcher::onChanged);
}

void CorpusWatcher::setMonitoredFiles(const QStringList &filePaths)
{
    clear();
    QSet<QString> dirs;
    for (const QString &p : filePaths) {
        const QFileInfo fi(p);
        if (!fi.exists())
            continue;
        const QString abs = fi.absoluteFilePath();
        m_files.append(abs);
        m_hashes.insert(abs, contentHash(abs));
        dirs.insert(fi.absolutePath());
    }
    if (!m_files.isEmpty())
        m_watcher->addPaths(m_files);
    if (!dirs.isEmpty())
        m_watcher->addPaths(dirs.values());
}

void CorpusWatcher::clear()
{
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_files.clear();
    m_hashes.clear();
}

void CorpusWatcher::onChanged()
{
    m_debounce->start();
}

void CorpusWatcher::finishDebounce()
{
    diffAndEmit();
}

void CorpusWatcher::diffAndEmit()
{
    const QHash<QString, QByteArray> oldHashes = m_hashes;

    // Current state of known files.
    QStringList gone;
    QStringList changed;
    for (const QString &f : m_files) {
        if (!QFileInfo::exists(f)) {
            gone.append(f);
        } else {
            const QByteArray h = contentHash(f);
            m_hashes.insert(f, h);
            if (h != oldHashes.value(f))
                changed.append(f);
        }
    }

    // Fresh (previously-unknown) files in watched dirs = rename candidates.
    QStringList fresh;
    for (const QString &dir : m_watcher->directories()) {
        for (const QFileInfo &e : QDir(dir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            const QString abs = e.absoluteFilePath();
            if (!m_files.contains(abs) && !fresh.contains(abs))
                fresh.append(abs);
        }
    }

    QStringList deletedUnpaired;
    for (const QString &from : gone) {
        QString partner;
        for (const QString &cand : fresh) {
            if (contentHash(cand) == oldHashes.value(from)) { partner = cand; break; }
        }
        if (!partner.isEmpty()) {
            m_files.replace(m_files.indexOf(from), partner);
            m_hashes.insert(partner, contentHash(partner));
            m_hashes.remove(from);
            emit renamed(from, partner);
        } else {
            deletedUnpaired.append(from);
        }
    }

    for (const QString &p : changed)
        emit edited(p);
    for (const QString &p : deletedUnpaired) {
        m_files.removeAll(p);
        m_hashes.remove(p);
        emit deleted(p);
    }
}
