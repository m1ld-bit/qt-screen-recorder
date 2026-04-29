QT       += core gui widgets multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = ScreenRecorder
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += \
    include \
    $$PWD/ffmpeg/include

LIBS += -L$$PWD/ffmpeg/lib \
    -lavcodec \
    -lavformat \
    -lavutil \
    -lswscale \
    -lswresample

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/ScreenRecorder.cpp \
    src/FFmpegEncoder.cpp \
    src/Logger.cpp

HEADERS += \
    include/MainWindow.h \
    include/ScreenRecorder.h \
    include/FFmpegEncoder.h \
    include/Logger.h

FORMS += \
    src/MainWindow.ui

RESOURCES += \
    resources/resources.qrc

RC_FILE = resources/app.rc

win32 {
    DEFINES += _CRT_SECURE_NO_WARNINGS
}

QMAKE_CXXFLAGS += /utf-8

