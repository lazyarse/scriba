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
#include <gtest/gtest.h>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVector>
#include "ZipReader.h"
#include "miniz.h"

namespace {

struct TestEntry {
    QString name;
    QByteArray data;
    mz_uint level;
};

// Build an on-disk ZIP at `path` with the given entries using miniz's writer.
// A directory entry is expressed by a name ending in '/' and empty data.
bool makeZip(const QString &path, const QVector<TestEntry> &entries)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        return false;
    for (const TestEntry &entry : entries) {
        if (!mz_zip_writer_add_mem(&zip, entry.name.toUtf8().constData(),
                                   entry.data.constData(), entry.data.size(),
                                   entry.level)) {
            mz_zip_writer_end(&zip);
            return false;
        }
    }
    void *buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) {
        mz_zip_writer_end(&zip);
        return false;
    }
    mz_zip_writer_end(&zip);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        mz_free(buf);
        return false;
    }
    file.write(static_cast<const char *>(buf), qint64(size));
    file.close();
    mz_free(buf);
    return true;
}

class ZipReaderTest : public ::testing::Test
{
protected:
    QTemporaryDir m_dir;
};

} // namespace

TEST_F(ZipReaderTest, OpenListsEntriesAndReadsBothMethods)
{
    const QByteArray deflateData =
        "word/document.xml content with UTF-8: Grüße, 你好, 🚀\n"
        "Second line to make it compress well. Second line to make it compress well.";
    const QByteArray storeData =
        "already-compressed payload: \x89PNG\r\n\x1a\n\x00\x00\x00\xff\xfe\xfd\x00\x00";

    ASSERT_TRUE(makeZip(m_dir.filePath(QStringLiteral("test.zip")), {
        { QStringLiteral("word/document.xml"), deflateData, MZ_BEST_COMPRESSION },
        { QStringLiteral("word/media/blob.bin"), storeData, MZ_NO_COMPRESSION },
        { QStringLiteral("empty_dir/"), QByteArray(), MZ_NO_COMPRESSION },
    }));

    ZipReader reader(m_dir.filePath(QStringLiteral("test.zip")));
    QString error;
    ASSERT_TRUE(reader.open(&error));

    const QStringList names = reader.entryNames();
    ASSERT_EQ(names.size(), 2);
    EXPECT_TRUE(names.contains(QStringLiteral("word/document.xml")));
    EXPECT_TRUE(names.contains(QStringLiteral("word/media/blob.bin")));

    EXPECT_TRUE(reader.hasEntry(QStringLiteral("word/document.xml")));
    EXPECT_TRUE(reader.hasEntry(QStringLiteral("word/media/blob.bin")));
    EXPECT_FALSE(reader.hasEntry(QStringLiteral("empty_dir/")));

    EXPECT_EQ(reader.readEntry(QStringLiteral("word/document.xml")), deflateData);
    EXPECT_EQ(reader.readEntry(QStringLiteral("word/media/blob.bin")), storeData);
    // Second read hits the cache.
    EXPECT_EQ(reader.readEntry(QStringLiteral("word/document.xml")), deflateData);
}

TEST_F(ZipReaderTest, NotAZipFailsWithError)
{
    const QString path = m_dir.filePath(QStringLiteral("not-a-zip.txt"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("this is not a zip archive\n");
    file.close();

    ZipReader reader(path);
    QString error;
    EXPECT_FALSE(reader.open(&error));
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(ZipReaderTest, CorruptEntryCrcReturnsEmpty)
{
    const QByteArray data = "crc check must catch corruption in this payload";
    ASSERT_TRUE(makeZip(m_dir.filePath(QStringLiteral("crc.zip")), {
        { QStringLiteral("payload.bin"), data, MZ_NO_COMPRESSION },
    }));

    // Flip one byte inside the stored data region so the CRC no longer matches.
    const QString path = m_dir.filePath(QStringLiteral("crc.zip"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    QByteArray bytes = file.readAll();
    const int dataAt = bytes.indexOf(data);
    ASSERT_GE(dataAt, 0);
    bytes[dataAt + data.size() - 1] = char(bytes.at(dataAt + data.size() - 1) ^ 0x01);
    file.seek(0);
    file.write(bytes);
    file.close();

    ZipReader reader(path);
    ASSERT_TRUE(reader.open());
    EXPECT_TRUE(reader.hasEntry(QStringLiteral("payload.bin")));
    EXPECT_TRUE(reader.readEntry(QStringLiteral("payload.bin")).isEmpty());
}

TEST_F(ZipReaderTest, ZipSlipNamesAreExcluded)
{
    ASSERT_TRUE(makeZip(m_dir.filePath(QStringLiteral("slip.zip")), {
        { QStringLiteral("../evil"), QByteArray("pwned"), MZ_BEST_COMPRESSION },
        { QStringLiteral("safe.txt"), QByteArray("ok"), MZ_NO_COMPRESSION },
    }));

    ZipReader reader(m_dir.filePath(QStringLiteral("slip.zip")));
    QString error;
    ASSERT_TRUE(reader.open(&error)) << qPrintable(error);

    const QStringList names = reader.entryNames();
    EXPECT_FALSE(names.contains(QStringLiteral("../evil")));
    EXPECT_TRUE(names.contains(QStringLiteral("safe.txt")));
    EXPECT_FALSE(reader.hasEntry(QStringLiteral("../evil")));
}
