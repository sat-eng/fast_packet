#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FastPacket");
    app.setApplicationVersion("2.0");
    MainWindow w;
    w.show();
    return app.exec();
}
