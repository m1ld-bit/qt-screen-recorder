#include "MainWindow.h"
#include "Logger.h"
#include <QApplication>
#include <QCoreApplication>
#include <QTextCodec>

int main(int argc, char* argv[])
{
    // 启用高DPI支持 - 必须在QApplication创建之前设置
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
    
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
#endif
    
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    a.setApplicationName("屏幕录制器");
    a.setOrganizationName("ScreenRecorder");

    Logger::instance();

    MainWindow w;
    w.show();

    int ret = a.exec();

    Logger::destroyInstance();
    return ret;
}

