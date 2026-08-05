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
#include <QSettings>

// Redirects all app config access to a per-process directory under the OS
// temp path (QDir::tempPath(), /tmp on Linux, %TEMP% on Windows) and wipes it
// so every test run starts clean. The directory is unique per process (keyed
// on the PID) so ctest can run test suites in parallel without them colliding
// on a shared config file. Must be called after the QApplication is created,
// before any test runs.
inline void setupTestConfig()
{
    QCoreApplication::setOrganizationName("scribaTest");
    QCoreApplication::setApplicationName("scribaTest");

    const QString unique = "scribaTest_" + QString::number(QCoreApplication::applicationPid());
    const QString testConfigDir = QDir::tempPath() + "/" + unique;

    // Force file-based settings (and into the per-process dir, above) so
    // Windows doesn't hit the shared registry and parallel runs stay isolated.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, testConfigDir);
    qputenv("SCRIBA_TEST_CONFIG_DIR", testConfigDir.toUtf8());

    QDir(testConfigDir).removeRecursively();
    QDir().mkpath(testConfigDir);
}
