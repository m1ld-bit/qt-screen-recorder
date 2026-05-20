QT       += core gui widgets multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Windows High DPI support
win32 {
    CONFIG += highdpi
    DEFINES += QT_NO_DEPRECATED_WARNINGS
}

TARGET = ScreenRecorder
TEMPLATE = app

# FFmpeg Configuration - ENABLED for MSVC
DEFINES += ENABLE_FFMPEG

contains(DEFINES, ENABLE_FFMPEG) {
    message(FFmpeg enabled)
    
    FFMPEG_DIR = $$PWD/ffmpeg
    INCLUDEPATH += \
        include \
        $$FFMPEG_DIR/include
    
    # FFmpeg libraries - auto-detect compiler (MSVC or MinGW)
    win32 {
        contains(QMAKE_CXX, g\\+\\+)|contains(QMAKE_CXX, clang) {
            # MinGW: use .dll.a files
            LIBS += -L$$FFMPEG_DIR/lib \
                -lavformat \
                -lavcodec \
                -lswresample \
                -lswscale \
                -lavutil
            message(MinGW detected - using .dll.a libraries)
        } else {
            # MSVC: use .lib files with full paths
            LIBS += \
                $$FFMPEG_DIR/lib/avformat.lib \
                $$FFMPEG_DIR/lib/avcodec.lib \
                $$FFMPEG_DIR/lib/swresample.lib \
                $$FFMPEG_DIR/lib/swscale.lib \
                $$FFMPEG_DIR/lib/avutil.lib
            message(MSV C detected - using .lib files)
        }
        
        # Windows system libraries required by FFmpeg
        LIBS += -lbcrypt -lOle32 -lUser32 -lMfplat -lStrmiids -lSecur32 -lShlwapi
        
        DEFINES += _CRT_SECURE_NO_WARNINGS
        DEFINES += __STDC_CONSTANT_MACROS
    }
    
    SOURCES += src/FFmpegEncoder.cpp
    HEADERS += include/FFmpegEncoder.h
} else {
    message(FFmpeg disabled - recording only, video will not be saved)
    INCLUDEPATH += include
}

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/ScreenRecorder.cpp \
    src/Logger.cpp \
    src/AreaSelector.cpp

HEADERS += \
    include/MainWindow.h \
    include/ScreenRecorder.h \
    include/Logger.h \
    include/AreaSelector.h

FORMS += \
    src/MainWindow.ui

RESOURCES += \
    resources/resources.qrc
