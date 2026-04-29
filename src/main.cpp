#include "MainWindow.h"
#include "Logger.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("屏幕录制器");
    a.setOrganizationName("ScreenRecorder");

    Logger::instance();

    MainWindow w;
    w.show();

    int ret = a.exec();

    Logger::destroyInstance();
    return ret;
}

