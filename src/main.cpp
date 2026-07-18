#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QStyleFactory>
#include "MainWindow.h"

Q_LOGGING_CATEGORY(lcApp, "scriba")

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
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
