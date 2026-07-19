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
    app.setApplicationName("Scriba");
    app.setOrganizationName("Scriba");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/scriba.svg"));

    if (app.arguments().contains("--debug"))
        QLoggingCategory::setFilterRules("scriba.*=true");

    MainWindow window;
    window.show();

    if (app.arguments().size() > 1) {
        QString arg = app.arguments().at(1);
        if (arg != "--debug")
            window.loadFile(arg);
    }

    return app.exec();
}
