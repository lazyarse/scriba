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
#include "ZipReader.h"
#include <QFile>
#include <QRegularExpression>
#include "miniz.h"

namespace {

constexpr quint32 kEocdSignature = 0x06054b50;
constexpr quint32 kCentralDirSignature = 0x02014b50;
constexpr quint32 kLocalHeaderSignature = 0x04034b50;

constexpr quint16 kEocdMinSize = 22;

// ZIP fields are little-endian. Reading the archive bytes into memory once and
// extracting offsets avoids per-entry QFile seeks for the central directory.
quint16 readU16(const char *p)
{
    return quint16(quint8(p[0])) | quint16(quint8(p[1])) << 8;
}

quint32 readU32(const char *p)
{
    return quint32(quint8(p[0])) | quint32(quint8(p[1])) << 8
         | quint32(quint8(p[2])) << 16 | quint32(quint8(p[3])) << 24;
}

bool isSignatureAt(const QByteArray &bytes, int offset, quint32 signature)
{
    return offset >= 0 && offset + 4 <= bytes.size()
        && readU32(bytes.constData() + offset) == signature;
}

// A trailing '/' marks a directory entry; skip those and any name that could
// escape the extraction root (absolute path or a `..` segment).
bool isUnsafeOrDirectoryName(const QString &name)
{
    if (name.isEmpty())
        return true;
    if (name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\')))
        return true;
    if (name.size() >= 2 && name.at(1) == QLatin1Char(':'))
        return true; // Windows drive letter
    if (name.endsWith(QLatin1Char('/')))
        return true;
    const QStringList parts = name.split(QRegularExpression(QStringLiteral("[/\\\\]")));
    for (const QString &part : parts) {
        if (part == QLatin1String(".."))
            return true;
    }
    return false;
}

} // namespace

ZipReader::ZipReader(const QString &filePath)
    : m_path(filePath)
{
}

bool ZipReader::open(QString *error)
{
    m_entries.clear();
    m_cache.clear();

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open %1: %2").arg(m_path, file.errorString());
        return false;
    }

    const qint64 fileSize = file.size();
    if (fileSize < kEocdMinSize) {
        if (error)
            *error = QStringLiteral("Not a ZIP archive (too small): %1").arg(m_path);
        return false;
    }

    // The End of Central Directory record is at most 22 bytes plus a 65535-byte
    // comment before the end of the file; scan the tail for its signature.
    const qint64 tailLen = qMin<qint64>(fileSize, kEocdMinSize + 65535);
    if (!file.seek(fileSize - tailLen)) {
        if (error)
            *error = QStringLiteral("Cannot read %1").arg(m_path);
        return false;
    }
    QByteArray tail = file.read(tailLen);
    if (tail.size() != tailLen) {
        if (error)
            *error = QStringLiteral("Cannot read %1").arg(m_path);
        return false;
    }

    int eocdOffset = -1;
    for (int i = tail.size() - kEocdMinSize; i >= 0; --i) {
        if (isSignatureAt(tail, i, kEocdSignature)) {
            const quint16 commentLen = readU16(tail.constData() + i + 20);
            if (i + kEocdMinSize + commentLen == tail.size()) {
                eocdOffset = i;
                break;
            }
        }
    }
    if (eocdOffset < 0) {
        if (error)
            *error = QStringLiteral("No ZIP central directory found: %1").arg(m_path);
        return false;
    }

    const quint16 numEntries = readU16(tail.constData() + eocdOffset + 10);
    const quint32 cdSize = readU32(tail.constData() + eocdOffset + 12);
    const quint32 cdOffset = readU32(tail.constData() + eocdOffset + 16);
    if (quint64(cdOffset) + cdSize > quint64(fileSize)) {
        if (error)
            *error = QStringLiteral("Corrupt central directory offset: %1").arg(m_path);
        return false;
    }

    if (!file.seek(cdOffset)) {
        if (error)
            *error = QStringLiteral("Cannot seek in %1").arg(m_path);
        return false;
    }
    QByteArray centralDir = file.read(cdSize);
    if (centralDir.size() != qint64(cdSize)) {
        if (error)
            *error = QStringLiteral("Truncated central directory: %1").arg(m_path);
        return false;
    }

    int pos = 0;
    for (quint16 i = 0; i < numEntries; ++i) {
        if (pos + 46 > centralDir.size() || !isSignatureAt(centralDir, pos, kCentralDirSignature)) {
            if (error)
                *error = QStringLiteral("Corrupt central directory entry %1: %2").arg(i).arg(m_path);
            return false;
        }
        const char *h = centralDir.constData() + pos;
        Entry e;
        e.method = readU16(h + 10);
        e.crc32 = readU32(h + 16);
        e.compSize = readU32(h + 20);
        e.uncompSize = readU32(h + 24);
        e.localOffset = readU32(h + 42);
        const quint16 nameLen = readU16(h + 28);
        const quint16 extraLen = readU16(h + 30);
        const quint16 commentLen = readU16(h + 32);

        if (pos + 46 + nameLen > centralDir.size()) {
            if (error)
                *error = QStringLiteral("Truncated central directory entry name: %1").arg(m_path);
            return false;
        }
        const QString name = QString::fromUtf8(centralDir.mid(pos + 46, nameLen));
        pos += 46 + nameLen + extraLen + commentLen;

        if (isUnsafeOrDirectoryName(name))
            continue;
        m_entries.insert(name, e);
    }

    return true;
}

QStringList ZipReader::entryNames() const
{
    return m_entries.keys();
}

bool ZipReader::hasEntry(const QString &name) const
{
    return m_entries.contains(name);
}

QByteArray ZipReader::readEntry(const QString &name) const
{
    auto cached = m_cache.constFind(name);
    if (cached != m_cache.constEnd())
        return cached.value();
    auto it = m_entries.constFind(name);
    if (it == m_entries.constEnd())
        return {};

    const Entry &e = it.value();
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    // Locate the compressed data by skipping the local header's fixed part
    // plus its filename/extra lengths (their content matches the central
    // directory but must be re-read since sizes are not stored there).
    if (!file.seek(e.localOffset))
        return {};
    char header[30];
    if (file.read(header, 30) != 30 || readU32(header) != kLocalHeaderSignature)
        return {};
    const quint16 localNameLen = readU16(header + 26);
    const quint16 localExtraLen = readU16(header + 28);
    if (!file.seek(e.localOffset + 30 + localNameLen + localExtraLen))
        return {};
    QByteArray compressed = file.read(e.compSize);
    if (compressed.size() != qint64(e.compSize))
        return {};

    QByteArray out;
    if (e.method == 8) {
        // RAW DEFLATE (no zlib header) is what ZIP stores; flags 0 for that.
        size_t outLen = 0;
        void *buf = tinfl_decompress_mem_to_heap(compressed.constData(), compressed.size(), &outLen, 0);
        if (!buf)
            return {};
        out = QByteArray(static_cast<const char *>(buf), int(outLen));
        mz_free(buf);
    } else if (e.method == 0) {
        out = compressed;
    } else {
        return {};
    }

    const mz_ulong actualCrc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>(out.constData()), out.size());
    if (actualCrc != e.crc32)
        return {};

    m_cache.insert(name, out);
    return out;
}
