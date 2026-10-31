#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PaintStation");

    MainWindow w;
    w.show();
    return app.exec();
}
