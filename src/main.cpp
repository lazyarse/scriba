#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Scriba");
    app.setOrganizationName("Scriba");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.show();

    if (app.arguments().size() > 1) {
        window.loadFile(app.arguments().at(1));
    }

    return app.exec();
}
