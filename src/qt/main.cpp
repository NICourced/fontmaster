#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("FontMaster");
    app.setApplicationVersion(FONTMASTER_VERSION);
    app.setOrganizationName("FontMaster");

    MainWindow window;
    window.show();

    return app.exec();
}
