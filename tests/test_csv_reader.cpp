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
#include "io/CsvReader.h"

TEST(CsvReaderTest, BasicHeaders)
{
    QString csv = "Name,Age,City\nAlice,30,NYC\nBob,25,LA\n";
    CsvData data = CsvReader::readFromString(csv, true);
    ASSERT_EQ(data.headers.size(), 3);
    EXPECT_EQ(data.headers[0], "Name");
    EXPECT_EQ(data.headers[1], "Age");
    EXPECT_EQ(data.headers[2], "City");
    ASSERT_EQ(data.rows.size(), 2);
    EXPECT_EQ(data.rows[0][0], "Alice");
    EXPECT_EQ(data.rows[0][1], "30");
    EXPECT_EQ(data.rows[1][0], "Bob");
}

TEST(CsvReaderTest, NoHeaders)
{
    QString csv = "Alice,30,NYC\nBob,25,LA\n";
    CsvData data = CsvReader::readFromString(csv, false);
    ASSERT_EQ(data.headers.size(), 3);
    EXPECT_EQ(data.headers[0], "Column 1");
    EXPECT_EQ(data.headers[1], "Column 2");
    EXPECT_EQ(data.headers[2], "Column 3");
    ASSERT_EQ(data.rows.size(), 2);
    EXPECT_EQ(data.rows[0][0], "Alice");
}

TEST(CsvReaderTest, QuotedFields)
{
    QString csv = "Name,Comment\nAlice,\"Hello, world\"\nBob,\"He said \"\"hi\"\"\"\n";
    CsvData data = CsvReader::readFromString(csv, true);
    ASSERT_EQ(data.rows.size(), 2);
    EXPECT_EQ(data.rows[0][1], "Hello, world");
    EXPECT_EQ(data.rows[1][1], "He said \"hi\"");
}

TEST(CsvReaderTest, EmptyValues)
{
    QString csv = "A,B,C\n1,,3\n,5,\n";
    CsvData data = CsvReader::readFromString(csv, true);
    ASSERT_EQ(data.rows.size(), 2);
    ASSERT_GE(data.rows[0].size(), 3);
    EXPECT_EQ(data.rows[0][0], "1");
    EXPECT_EQ(data.rows[0][1], "");
    EXPECT_EQ(data.rows[0][2], "3");
}

TEST(CsvReaderTest, EmptyInput)
{
    CsvData data = CsvReader::readFromString("", true);
    EXPECT_TRUE(data.headers.isEmpty());
    EXPECT_TRUE(data.rows.isEmpty());
}

TEST(CsvReaderTest, SingleRow)
{
    QString csv = "X,Y\n42,hello\n";
    CsvData data = CsvReader::readFromString(csv, true);
    ASSERT_EQ(data.headers.size(), 2);
    ASSERT_EQ(data.rows.size(), 1);
    EXPECT_EQ(data.rows[0][0], "42");
    EXPECT_EQ(data.rows[0][1], "hello");
}

TEST(CsvReaderTest, TrailingNewline)
{
    QString csv = "A,B\n1,2\n3,4\n";
    CsvData data = CsvReader::readFromString(csv, true);
    ASSERT_EQ(data.rows.size(), 2);
    EXPECT_EQ(data.rows[1][0], "3");
    EXPECT_EQ(data.rows[1][1], "4");
}

TEST(CsvReaderTest, NoHeadersSingleRow)
{
    QString csv = "only,one,row\n";
    CsvData data = CsvReader::readFromString(csv, false);
    ASSERT_EQ(data.headers.size(), 3);
    ASSERT_EQ(data.rows.size(), 1);
    EXPECT_EQ(data.rows[0][0], "only");
    EXPECT_EQ(data.rows[0][2], "row");
}
