QT       += core gui widgets multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = ScreenRecorder
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

# FFmpeg 配置 - 取消下面的注释以启用FFmpeg
# 建议将FFmpeg放在项目目录下的ffmpeg文件夹中
# ffmpeg/
#   include/   (头文件)
#   lib/       (库文件)
#   bin/       (dll文件，运行时需要)

# 启用FFmpeg支持（取消下面这行的注释）
# DEFINES += ENABLE_FFMPEG

contains(DEFINES, ENABLE_FFMPEG) {
    message(使用FFmpeg编码器)
    
    # FFmpeg 头文件路径 - 根据你的实际路径修改
    FFMPEG_DIR = $$PWD/ffmpeg
    INCLUDEPATH += \
        include \
        $$FFMPEG_DIR/include
    
    # FFmpeg 库文件 - Windows x64
    win32: LIBS += -L$$FFMPEG_DIR/lib \
        -lavcodec \
        -lavformat \
        -lavutil \
        -lswscale \
        -lswresample \
        -lavdevice
    
    # Windows特定定义
    win32 {
        DEFINES += _CRT_SECURE_NO_WARNINGS
        DEFINES += __STDC_CONSTANT_MACROS
    }
    
    SOURCES += src/FFmpegEncoder.cpp
    HEADERS += include/FFmpegEncoder.h
} else {
    message(FFmpeg编码器已禁用，仅录制不保存)
    INCLUDEPATH += include
}

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

