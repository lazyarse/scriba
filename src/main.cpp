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
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtWebEngineCore/QWebEngineUrlScheme>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif
#include "MainWindow.h"
#include "LogWindow.h"
#include "Preferences.h"

static void registerQrcScheme()
{
    // Must run before any Qt WebEngine class is created. Qt registers the qrc
    // scheme with only SecureScheme|ViewSourceAllowed, which CORS-blocks fonts
    // (Symbola, KaTeX) loaded via @font-face from pages with a file:// base URL
    // (live preview, PDF/DOCX export). Pre-registering with CorsEnabled and
    // LocalAccessAllowed restores the Qt 5 behavior per Qt's documented recipe.
    QWebEngineUrlScheme qrcScheme(QByteArrayLiteral("qrc"));
    qrcScheme.setFlags(QWebEngineUrlScheme::SecureScheme
                       | QWebEngineUrlScheme::LocalAccessAllowed
                       | QWebEngineUrlScheme::CorsEnabled
                       | QWebEngineUrlScheme::ViewSourceAllowed);
    QWebEngineUrlScheme::registerScheme(qrcScheme);
}

int main(int argc, char *argv[])
{
    registerQrcScheme();

    QApplication app(argc, argv);

#ifndef Q_OS_WIN
    if (geteuid() == 0) {
        QMessageBox::critical(nullptr, "Scriba",
            "Scriba should not be run as root.\n\n"
            "Please run as your normal user. If you need to\n"
            "edit a protected file, use: sudo -e file.md");
        return 1;
    }
#endif

    app.setStyle(QStyleFactory::create("Fusion"));
    app.setOrganizationName("scriba");
    app.setApplicationName("scriba");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/scriba.svg"));

    LogWindow::initDebugLogging();

    QSettings settings;
    Preferences::migrateSettings(settings);

    QStringList args = app.arguments();
    bool hasFiles = false;
    for (int i = 1; i < args.size(); ++i) {
        hasFiles = true;
        break;
    }

    MainWindow::setNotifyStaleCss(true);
    MainWindow window(nullptr, hasFiles);
    window.showMaximized();

    if (hasFiles) {
        for (int i = 1; i < args.size(); ++i)
            window.loadFile(args[i]);
    }

    return app.exec();
}
