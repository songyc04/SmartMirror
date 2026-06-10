#include "mainwindow.h"
#include <QApplication>
#include <QProcess>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Pretendard 폰트 로드 및 전역 적용
    int fontId = QFontDatabase::addApplicationFont(":/fonts/Pretendard-Medium.otf");
    if (fontId != -1)
    {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
        {
            QFont appFont(families.at(0));
            appFont.setPointSize(12);
            a.setFont(appFont);
        }
    }

    QProcess::execute("xset", QStringList() << "dpms" << "force" << "off");

    MainWindow w;
    w.showFullScreen();

    return a.exec();
}
