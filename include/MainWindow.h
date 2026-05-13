#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QAudioInput>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include "ScreenRecorder.h"

#ifdef ENABLE_FFMPEG
#include "FFmpegEncoder.h"
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // 录制控制槽函数
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    
    // 录制模式切换
    void onFullScreenToggled(bool checked);
    void onAreaToggled(bool checked);
    void onSelectAreaClicked();
    
    // 音频控制
    void onAudioToggled(bool checked);
    void updateAudioLevel();
    
    // 设置相关
    void onFpsChanged(int index);
    void onQualityChanged(int index);
    void onBrowseOutputPath();
    void onOpenFolder();
    
    // 系统托盘
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayStart();
    void onTrayStop();
    void onTraySettings();
    void onTrayExit();
    
    // 快捷键
    void onToggleRecording();
    
    // 录制器状态更新 - 适配新接口
    void onStateChanged(ScreenRecorder::RecordState state);
    void onNewFrameAvailable(QSharedPointer<ScreenRecorder::Frame> frame);
    void onScreenConfigChanged();
    void onRecorderError(const QString& error);
    
#ifdef ENABLE_FFMPEG
    // 编码器状态更新
    void onEncoderError(const QString& error);
    void onEncodingProgress(qint64 videoFrames, qint64 audioFrames);
#endif
    
    // 更新录制时间
    void updateElapsedTime();

private:
    // UI初始化
    void initUI();
    void initTrayIcon();
    void initShortcuts();
    void initAudioMonitor();
    
    // UI更新
    void updateUI();
    void updateTrayIcon();
    void updateButtonStyles();
    void updateStatusBar();
    
    // 辅助功能
    void showFloatingIndicator();
    void hideFloatingIndicator();
    void loadSettings();
    void saveSettings();
    QString generateFileName();
    QString formatFileSize(qint64 bytes);
    QString getStatusText(ScreenRecorder::RecordState state);
    
    // 配置转换
    void applyConfigToRecorder();

    Ui::MainWindow* ui;
    ScreenRecorder* m_recorder;
    
#ifdef ENABLE_FFMPEG
    FFmpegEncoder* m_encoder;
#endif
    
    // 系统托盘
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QAction* m_trayStartAction;
    QAction* m_trayStopAction;
    QAction* m_traySettingsAction;
    QAction* m_trayExitAction;
    
    // 快捷键
    QShortcut* m_recordShortcut;
    
    // 悬浮窗口
    QWidget* m_floatingWidget;
    QLabel* m_floatingLabel;
    
    // 音频监控
    QAudioInput* m_audioMonitor;
    QIODevice* m_audioDevice;
    QTimer* m_audioLevelTimer;
    qreal m_currentAudioLevel;
    
    // 录制状态
    bool m_isSelectingArea;
    QPoint m_selectionStart;
    QRect m_selectedRect;
    qint64 m_elapsedMs;
    qint64 m_fileSize;
    QString m_currentFilePath;
    
    // 计时器
    QTimer* m_elapsedTimer;
    qint64 m_recordStartTime;
};

#endif // MAINWINDOW_H
