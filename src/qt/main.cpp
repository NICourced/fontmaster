#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Настройка приложения
    app.setApplicationName("FontMaster");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("FontMaster");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
