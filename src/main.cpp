#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStyleFactory>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif
#include "MainWindow.h"
#include "LogWindow.h"

int main(int argc, char *argv[])
{
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

    QStringList args = app.arguments();
    bool hasFiles = false;
    for (int i = 1; i < args.size(); ++i) {
        hasFiles = true;
        break;
    }

    MainWindow window(nullptr, hasFiles);
    window.showMaximized();

    if (hasFiles) {
        for (int i = 1; i < args.size(); ++i)
            window.loadFile(args[i]);
    }

    return app.exec();
}
