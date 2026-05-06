QT       += core gui widgets multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = ScreenRecorder
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += \
    include

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/ScreenRecorder.cpp \
    src/Logger.cpp

HEADERS += \
    include/MainWindow.h \
    include/ScreenRecorder.h \
    include/Logger.h

FORMS += \
    src/MainWindow.ui

RESOURCES += \
    resources/resources.qrc

win32 {
    DEFINES += _CRT_SECURE_NO_WARNINGS
}

