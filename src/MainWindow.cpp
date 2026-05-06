#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Logger.h"
#include <QFileDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QScreen>
#include <QPainter>
#include <QCloseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QStandardPaths>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QFile>
#include <QDebug>
#include <QDateTime>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_recorder(nullptr)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_trayStartAction(nullptr)
    , m_trayStopAction(nullptr)
    , m_traySettingsAction(nullptr)
    , m_trayExitAction(nullptr)
    , m_recordShortcut(nullptr)
    , m_floatingWidget(nullptr)
    , m_floatingLabel(nullptr)
    , m_audioMonitor(nullptr)
    , m_audioDevice(nullptr)
    , m_audioLevelTimer(nullptr)
    , m_currentAudioLevel(0.0)
    , m_isSelectingArea(false)
    , m_elapsedMs(0)
    , m_fileSize(0)
    , m_elapsedTimer(nullptr)
    , m_recordStartTime(0)
{
    ui->setupUi(this);

    m_recorder = new ScreenRecorder(this);

    initUI();
    initTrayIcon();
    initShortcuts();
    initAudioMonitor();
    loadSettings();

    // 连接信号槽 - 使用队列连接确保跨线程安全
    connect(m_recorder, &ScreenRecorder::stateChanged, this, &MainWindow::onStateChanged, Qt::QueuedConnection);
    connect(m_recorder, &ScreenRecorder::newFrameAvailable, this, &MainWindow::onNewFrameAvailable, Qt::QueuedConnection);
    connect(m_recorder, &ScreenRecorder::screenConfigChanged, this, &MainWindow::onScreenConfigChanged, Qt::QueuedConnection);
    connect(m_recorder, &ScreenRecorder::errorOccurred, this, &MainWindow::onRecorderError, Qt::QueuedConnection);

    LOG_INFO("Application started");
}

MainWindow::~MainWindow()
{
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
        delete m_elapsedTimer;
    }
    if (m_audioLevelTimer) {
        m_audioLevelTimer->stop();
        delete m_audioLevelTimer;
    }
    if (m_audioMonitor) {
        m_audioMonitor->stop();
        delete m_audioMonitor;
    }
    delete ui;
}

void MainWindow::initUI()
{
    // 窗口设置
    setWindowTitle("屏幕录制器");
    setMinimumSize(480, 620);
    setMaximumSize(600, 700);
    
    // 启用高DPI支持
    setAttribute(Qt::AA_EnableHighDpiScaling);
    setAttribute(Qt::AA_UseHighDpiPixmaps);

    // 初始化录制模式
    ui->fullScreenRadio->setChecked(true);
    ui->selectAreaBtn->setEnabled(false);

    // 初始化帧率
    ui->fpsCombo->addItem("15 FPS", 15);
    ui->fpsCombo->addItem("30 FPS", 30);
    ui->fpsCombo->addItem("60 FPS", 60);
    ui->fpsCombo->setCurrentIndex(1);

    // 初始化画质
    ui->qualityCombo->addItem("低画质", 0);
    ui->qualityCombo->addItem("中等画质", 1);
    ui->qualityCombo->addItem("高画质", 2);
    ui->qualityCombo->setCurrentIndex(1);

    // 初始化显示器列表
    QList<QScreen*> screens = ScreenRecorder::getScreens();
    for (int i = 0; i < screens.size(); ++i) {
        ui->screenCombo->addItem(QString("显示器 %1").arg(i + 1), i);
    }

    // 初始化音频
    ui->audioProgress->setRange(0, 100);
    ui->audioProgress->setValue(0);
    ui->audioProgress->setEnabled(false);

    // 设置默认保存路径为桌面
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    ui->outputPathEdit->setText(desktopPath);

    // 初始化状态栏
    ui->statusTimeLabel->setText("00:00:00");
    ui->statusSizeLabel->setText("0 MB");
    ui->statusStateLabel->setText("空闲");

    // 初始化时间计时器
    m_elapsedTimer = new QTimer(this);
    connect(m_elapsedTimer, &QTimer::timeout, this, &MainWindow::updateElapsedTime);

    // 连接信号槽
    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(ui->fullScreenRadio, &QRadioButton::toggled, this, &MainWindow::onFullScreenToggled);
    connect(ui->areaRadio, &QRadioButton::toggled, this, &MainWindow::onAreaToggled);
    connect(ui->selectAreaBtn, &QPushButton::clicked, this, &MainWindow::onSelectAreaClicked);
    connect(ui->audioCheck, &QCheckBox::toggled, this, &MainWindow::onAudioToggled);
    connect(ui->fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFpsChanged);
    connect(ui->qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onQualityChanged);
    connect(ui->browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputPath);
    connect(ui->openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolder);

    updateUI();
    updateButtonStyles();
}

void MainWindow::initTrayIcon()
{
    // 创建托盘菜单
    m_trayMenu = new QMenu(this);
    
    m_trayStartAction = m_trayMenu->addAction("开始录制");
    m_trayStopAction = m_trayMenu->addAction("停止录制");
    m_trayStopAction->setEnabled(false);
    m_trayMenu->addSeparator();
    m_traySettingsAction = m_trayMenu->addAction("设置");
    m_trayMenu->addSeparator();
    m_trayExitAction = m_trayMenu->addAction("退出");

    // 连接托盘菜单信号
    connect(m_trayStartAction, &QAction::triggered, this, &MainWindow::onTrayStart);
    connect(m_trayStopAction, &QAction::triggered, this, &MainWindow::onTrayStop);
    connect(m_traySettingsAction, &QAction::triggered, this, &MainWindow::onTraySettings);
    connect(m_trayExitAction, &QAction::triggered, this, &MainWindow::onTrayExit);

    // 创建托盘图标
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("屏幕录制器 - 空闲");
    
    // 使用默认图标（实际项目中应该添加图标资源）
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

void MainWindow::initShortcuts()
{
    m_recordShortcut = new QShortcut(QKeySequence("Ctrl+Shift+R"), this);
    connect(m_recordShortcut, &QShortcut::activated, this, &MainWindow::onToggleRecording);
}

void MainWindow::initAudioMonitor()
{
    m_audioLevelTimer = new QTimer(this);
    connect(m_audioLevelTimer, &QTimer::timeout, this, &MainWindow::updateAudioLevel);
}

void MainWindow::onStartClicked()
{
    LOG_INFO("Start recording clicked");

    // 应用配置到录制器
    applyConfigToRecorder();

    // 生成输出文件路径
    m_currentFilePath = ui->outputPathEdit->text() + "/" + generateFileName();

    // 开始采集
    if (m_recorder->start()) {
        m_recordStartTime = QDateTime::currentMSecsSinceEpoch();
        m_elapsedTimer->start(100);
        showFloatingIndicator();
        saveSettings();
    }
}

void MainWindow::onPauseClicked()
{
    LOG_INFO("Pause recording clicked");

    ScreenRecorder::RecordState state = m_recorder->state();
    if (state == ScreenRecorder::Recording) {
        m_recorder->pause();
        m_elapsedTimer->stop();
    } else if (state == ScreenRecorder::Paused) {
        m_recorder->resume();
        m_elapsedTimer->start();
    }
}

void MainWindow::onStopClicked()
{
    LOG_INFO("Stop recording clicked");

    m_recorder->stop();
    m_elapsedTimer->stop();
    hideFloatingIndicator();
    
    // 这里应该触发编码保存（当前ScreenRecorder只负责采集）
    QMessageBox::information(this, "录制完成", 
                           QString("录制已完成！\n\n帧队列中有 %1 帧等待处理。").arg(m_recorder->queueSize()));
}

void MainWindow::onFullScreenToggled(bool checked)
{
    if (checked) {
        ui->selectAreaBtn->setEnabled(false);
        ui->screenCombo->setEnabled(true);
    }
}

void MainWindow::onAreaToggled(bool checked)
{
    if (checked) {
        ui->selectAreaBtn->setEnabled(true);
        ui->screenCombo->setEnabled(false);
    }
}

void MainWindow::onSelectAreaClicked()
{
    // 简单的区域选择提示（实际应该实现区域选择窗口）
    QMessageBox::information(this, "区域选择", 
                           "区域选择功能将在后续版本中实现。\n当前将使用默认区域。");
    
    // 设置一个示例区域
    QScreen* screen = ScreenRecorder::getPrimaryScreen();
    if (screen) {
        QRect geom = screen->geometry();
        m_selectedRect = QRect(geom.center().x() - 400, geom.center().y() - 300, 800, 600);
    }
}

void MainWindow::onAudioToggled(bool checked)
{
    ui->audioProgress->setEnabled(checked);
    
    if (checked) {
        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(format)) {
            format = info.nearestFormat(format);
        }

        m_audioMonitor = new QAudioInput(format, this);
        m_audioDevice = m_audioMonitor->start();
        m_audioLevelTimer->start(50);
    } else {
        m_audioLevelTimer->stop();
        if (m_audioMonitor) {
            m_audioMonitor->stop();
            delete m_audioMonitor;
            m_audioMonitor = nullptr;
        }
        ui->audioProgress->setValue(0);
    }
}

void MainWindow::updateAudioLevel()
{
    if (!m_audioDevice) return;

    QByteArray data = m_audioDevice->readAll();
    if (data.isEmpty()) return;

    qint16 maxValue = 0;
    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / 2;

    for (int i = 0; i < sampleCount; ++i) {
        qint16 absValue = qAbs(samples[i]);
        if (absValue > maxValue) {
            maxValue = absValue;
        }
    }

    int level = (maxValue * 100) / 32767;
    ui->audioProgress->setValue(level);
}

void MainWindow::onFpsChanged(int index)
{
    int fps = ui->fpsCombo->itemData(index).toInt();
    
    // 如果正在录制，实时调整帧率
    if (m_recorder->state() == ScreenRecorder::Recording) {
        m_recorder->setFrameRate(fps);
    }
    
    LOG_INFO(QString("FPS changed to %1").arg(fps));
}

void MainWindow::onQualityChanged(int index)
{
    Q_UNUSED(index);
    // 画质设置将在编码阶段使用
}

void MainWindow::onBrowseOutputPath()
{
    QString path = QFileDialog::getExistingDirectory(this, "选择保存目录", ui->outputPathEdit->text());
    if (!path.isEmpty()) {
        ui->outputPathEdit->setText(path);
    }
}

void MainWindow::onOpenFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(ui->outputPathEdit->text()));
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::onTrayStart()
{
    onStartClicked();
}

void MainWindow::onTrayStop()
{
    onStopClicked();
}

void MainWindow::onTraySettings()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::onTrayExit()
{
    close();
}

void MainWindow::onToggleRecording()
{
    ScreenRecorder::RecordState state = m_recorder->state();
    if (state == ScreenRecorder::Idle) {
        onStartClicked();
    } else if (state == ScreenRecorder::Recording) {
        onPauseClicked();
    }
}

void MainWindow::onStateChanged(ScreenRecorder::RecordState state)
{
    LOG_INFO(QString("Recorder state changed: %1").arg(getStatusText(state)));
    updateUI();
    updateButtonStyles();
    updateStatusBar();
    updateTrayIcon();
}

void MainWindow::onNewFrameAvailable(QSharedPointer<ScreenRecorder::Frame> frame)
{
    // 这里可以做帧预览或统计
    m_fileSize += frame->image.byteCount();
    updateStatusBar();
}

void MainWindow::onScreenConfigChanged()
{
    LOG_WARN("Screen configuration changed!");
    QMessageBox::warning(this, "屏幕配置变化", 
                        "检测到屏幕分辨率或显示器配置变化。\n全屏录制已自动调整到新的屏幕尺寸。");
}

void MainWindow::onRecorderError(const QString& error)
{
    QMessageBox::critical(this, "录制错误", error);
}

void MainWindow::updateElapsedTime()
{
    m_elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_recordStartTime;
    updateStatusBar();
}

void MainWindow::updateUI()
{
    ScreenRecorder::RecordState state = m_recorder->state();
    
    bool isRecording = (state == ScreenRecorder::Recording);
    bool isPaused = (state == ScreenRecorder::Paused);
    bool isIdle = (state == ScreenRecorder::Idle);

    // 更新按钮启用状态
    ui->startBtn->setEnabled(isIdle);
    ui->pauseBtn->setEnabled(isRecording || isPaused);
    ui->stopBtn->setEnabled(isRecording || isPaused);
    
    // 更新设置区域
    ui->fullScreenRadio->setEnabled(isIdle);
    ui->areaRadio->setEnabled(isIdle);
    ui->selectAreaBtn->setEnabled(isIdle && ui->areaRadio->isChecked());
    ui->screenCombo->setEnabled(isIdle);
    ui->fpsCombo->setEnabled(true);  // 帧率可以实时调整
    ui->qualityCombo->setEnabled(isIdle);
    ui->outputPathEdit->setEnabled(isIdle);
    ui->browseBtn->setEnabled(isIdle);
    ui->audioCheck->setEnabled(isIdle);
}

void MainWindow::updateButtonStyles()
{
    ScreenRecorder::RecordState state = m_recorder->state();
    
    // 清除所有样式
    ui->startBtn->setStyleSheet("");
    ui->pauseBtn->setStyleSheet("");
    ui->stopBtn->setStyleSheet("");

    if (state == ScreenRecorder::Recording) {
        ui->pauseBtn->setStyleSheet(
            "QPushButton { background-color: #FFC107; color: white; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton:hover { background-color: #E0A800; }"
        );
        ui->stopBtn->setStyleSheet(
            "QPushButton { background-color: #F44336; color: white; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton:hover { background-color: #D32F2F; }"
        );
    } else if (state == ScreenRecorder::Paused) {
        ui->pauseBtn->setStyleSheet(
            "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton:hover { background-color: #45A049; }"
        );
        ui->stopBtn->setStyleSheet(
            "QPushButton { background-color: #F44336; color: white; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton:hover { background-color: #D32F2F; }"
        );
    } else {
        ui->startBtn->setStyleSheet(
            "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton:hover { background-color: #45A049; }"
        );
    }

    // 更新暂停按钮文本
    ui->pauseBtn->setText(state == ScreenRecorder::Paused ? "继续" : "暂停");
}

void MainWindow::updateStatusBar()
{
    ScreenRecorder::RecordState state = m_recorder->state();
    
    // 更新时间
    int seconds = m_elapsedMs / 1000;
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    seconds = seconds % 60;
    ui->statusTimeLabel->setText(QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));

    // 更新文件大小
    ui->statusSizeLabel->setText(formatFileSize(m_fileSize));

    // 更新状态
    ui->statusStateLabel->setText(getStatusText(state));
}

void MainWindow::updateTrayIcon()
{
    ScreenRecorder::RecordState state = m_recorder->state();
    
    QString stateText = getStatusText(state);
    m_trayIcon->setToolTip(QString("屏幕录制器 - %1").arg(stateText));
    
    m_trayStartAction->setEnabled(state == ScreenRecorder::Idle);
    m_trayStopAction->setEnabled(state == ScreenRecorder::Recording || state == ScreenRecorder::Paused);
}

void MainWindow::showFloatingIndicator()
{
    if (!m_floatingWidget) {
        m_floatingWidget = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        m_floatingWidget->setAttribute(Qt::WA_TranslucentBackground);
        
        QVBoxLayout* layout = new QVBoxLayout(m_floatingWidget);
        m_floatingLabel = new QLabel("● 录制中", m_floatingWidget);
        m_floatingLabel->setStyleSheet(
            "QLabel { background-color: rgba(244, 67, 54, 0.8); color: white; "
            "padding: 8px 16px; border-radius: 4px; font-weight: bold; }"
        );
        layout->addWidget(m_floatingLabel);
        m_floatingWidget->setLayout(layout);
    }
    
    QScreen* screen = ScreenRecorder::getPrimaryScreen();
    if (screen) {
        m_floatingWidget->move(screen->geometry().topLeft() + QPoint(20, 20));
    }
    m_floatingWidget->show();
}

void MainWindow::hideFloatingIndicator()
{
    if (m_floatingWidget) {
        m_floatingWidget->hide();
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("ScreenRecorder", "Settings");
    
    ui->fpsCombo->setCurrentIndex(settings.value("fpsIndex", 1).toInt());
    ui->qualityCombo->setCurrentIndex(settings.value("qualityIndex", 1).toInt());
    ui->outputPathEdit->setText(settings.value("outputPath", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).toString());
    ui->audioCheck->setChecked(settings.value("audioEnabled", false).toBool());
    
    // 恢复窗口位置
    restoreGeometry(settings.value("windowGeometry").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings("ScreenRecorder", "Settings");
    
    settings.setValue("fpsIndex", ui->fpsCombo->currentIndex());
    settings.setValue("qualityIndex", ui->qualityCombo->currentIndex());
    settings.setValue("outputPath", ui->outputPathEdit->text());
    settings.setValue("audioEnabled", ui->audioCheck->isChecked());
    settings.setValue("windowGeometry", saveGeometry());
}

QString MainWindow::generateFileName()
{
    return QString("ScreenRecord_%1.mp4").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString("%1 GB").arg((double)bytes / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg((double)bytes / MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg((double)bytes / KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString MainWindow::getStatusText(ScreenRecorder::RecordState state)
{
    switch (state) {
        case ScreenRecorder::Idle:
            return "空闲";
        case ScreenRecorder::Recording:
            return "录制中";
        case ScreenRecorder::Paused:
            return "已暂停";
        default:
            return "未知状态";
    }
}

void MainWindow::applyConfigToRecorder()
{
    ScreenRecorder::CaptureConfig config;
    
    config.fullScreen = ui->fullScreenRadio->isChecked();
    config.frameRate = ui->fpsCombo->itemData(ui->fpsCombo->currentIndex()).toInt();
    config.screenIndex = ui->screenCombo->itemData(ui->screenCombo->currentIndex()).toInt();
    config.maxQueueSize = 30;
    
    // 设置录制区域
    if (config.fullScreen) {
        QList<QScreen*> screens = ScreenRecorder::getScreens();
        if (config.screenIndex >= 0 && config.screenIndex < screens.size()) {
            config.recordRect = screens[config.screenIndex]->geometry();
        } else {
            QScreen* primary = ScreenRecorder::getPrimaryScreen();
            if (primary) {
                config.recordRect = primary->geometry();
            }
        }
    } else {
        config.recordRect = m_selectedRect;
        if (config.recordRect.isEmpty()) {
            // 如果没有选择区域，使用默认区域
            QScreen* primary = ScreenRecorder::getPrimaryScreen();
            if (primary) {
                QRect geom = primary->geometry();
                config.recordRect = QRect(geom.center().x() - 400, geom.center().y() - 300, 800, 600);
            }
        }
    }
    
    m_recorder->setConfig(config);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    ScreenRecorder::RecordState state = m_recorder->state();
    
    if (state != ScreenRecorder::Idle) {
        auto reply = QMessageBox::question(this, "退出确认", 
                                         "正在录制中，确定要退出吗？",
                                         QMessageBox::Yes | QMessageBox::No,
                                         QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_recorder->stop();
            hideFloatingIndicator();
            saveSettings();
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        saveSettings();
        event->accept();
    }
}
