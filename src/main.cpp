#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QStyleFactory>
#include <unistd.h>
#include "MainWindow.h"

Q_LOGGING_CATEGORY(lcApp, "scriba")

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (geteuid() == 0) {
        QMessageBox::critical(nullptr, "Scriba",
            "Scriba should not be run as root.\n\n"
            "Please run as your normal user. If you need to\n"
            "edit a protected file, use: sudo -e file.md");
        return 1;
    }

    app.setStyle(QStyleFactory::create("Fusion"));
    app.setOrganizationName("scriba");
    app.setApplicationName("scriba");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/scriba.svg"));

    if (app.arguments().contains("--debug"))
        QLoggingCategory::setFilterRules("scriba.*=true");

    MainWindow window;
    window.show();

    QStringList args = app.arguments();
    bool hasFiles = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--debug")
            continue;
        window.loadFile(args[i]);
        hasFiles = true;
    }

    return app.exec();
}
