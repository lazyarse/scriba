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

#include <QCoreApplication>
#include <QDir>

// Redirects all app config access to a dedicated test directory
// (~/.config/scribaTest) and wipes it so every test run starts clean.
// Must be called after the QApplication is created, before any test runs.
inline void setupTestConfig()
{
    QCoreApplication::setOrganizationName("scribaTest");
    QCoreApplication::setApplicationName("scribaTest");
    const QString testConfigDir = QDir::homePath() + "/.config/scribaTest";
    qputenv("SCRIBA_TEST_CONFIG_DIR", testConfigDir.toUtf8());
    QDir(testConfigDir).removeRecursively();
    QDir().mkpath(testConfigDir);
}
