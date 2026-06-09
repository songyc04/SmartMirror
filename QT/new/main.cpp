#include "mainwindow.h"
#include <QApplication>
#include <QProcess>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QProcess::execute("xset", QStringList() << "dpms" << "force" << "off");

    MainWindow w;
    w.showFullScreen();

    return a.exec();
}
