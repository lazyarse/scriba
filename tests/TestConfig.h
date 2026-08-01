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
