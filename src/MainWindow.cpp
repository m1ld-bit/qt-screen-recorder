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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_recorder(nullptr)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_recordShortcut(nullptr)
    , m_floatingWidget(nullptr)
    , m_floatingLabel(nullptr)
    , m_isSelectingArea(false)
{
    ui->setupUi(this);

    m_recorder = new ScreenRecorder(this);

    initUI();
    initTrayIcon();
    initShortcuts();
    loadSettings();

    connect(m_recorder, &ScreenRecorder::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_recorder, &ScreenRecorder::countdownUpdated, this, &MainWindow::onCountdownUpdated);
    connect(m_recorder, &ScreenRecorder::elapsedTimeUpdated, this, &MainWindow::onElapsedTimeUpdated);
    connect(m_recorder, &ScreenRecorder::recordingFinished, this, &MainWindow::onRecordingFinished);
    connect(m_recorder, &ScreenRecorder::errorOccurred, this, &MainWindow::onRecorderError);

    LOG_INFO("Application started");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    setWindowTitle("屏幕录制器");
    setFixedSize(400, 500);

    ui->fullScreenRadio->setChecked(true);
    ui->fpsCombo->addItem("15 FPS", 15);
    ui->fpsCombo->addItem("30 FPS", 30);
    ui->fpsCombo->addItem("60 FPS", 60);
    ui->fpsCombo->setCurrentIndex(1);

    ui->qualityCombo->addItem("低画质", 0);
    ui->qualityCombo->addItem("中等画质", 1);
    ui->qualityCombo->addItem("高画质", 2);
    ui->qualityCombo->setCurrentIndex(1);

    QList<QScreen*> screens = ScreenRecorder::getScreens();
    for (int i = 0; i < screens.size(); ++i) {
        ui->screenCombo->addItem(QString("显示器 %1").arg(i + 1), i);
    }

    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(ui->selectAreaBtn, &QPushButton::clicked, this, &MainWindow::onSelectAreaClicked);
    connect(ui->browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputPath);
    connect(ui->openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolder);

    updateUI();
}

void MainWindow::initTrayIcon()
{
    m_trayMenu = new QMenu(this);
    QAction* showAction = m_trayMenu->addAction("显示");
    QAction* toggleAction = m_trayMenu->addAction("开始/停止录制");
    m_trayMenu->addSeparator();
    QAction* quitAction = m_trayMenu->addAction("退出");

    connect(showAction, &QAction::triggered, this, [this]() { show(); raise(); activateWindow(); });
    connect(toggleAction, &QAction::triggered, this, &MainWindow::onToggleRecording);
    connect(quitAction, &QAction::triggered, this, [this]() { saveSettings(); qApp->quit(); });

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("屏幕录制器");
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

void MainWindow::initShortcuts()
{
    m_recordShortcut = new QShortcut(QKeySequence("Ctrl+Shift+R"), this);
    connect(m_recordShortcut, &QShortcut::activated, this, &MainWindow::onToggleRecording);
}

void MainWindow::updateUI()
{
    ScreenRecorder::RecordState state = m_recorder->state();

    bool isIdle = state == ScreenRecorder::Idle;
    bool isRecording = state == ScreenRecorder::Recording;
    bool isPaused = state == ScreenRecorder::Paused;

    ui->startBtn->setEnabled(isIdle);
    ui->pauseBtn->setEnabled(isRecording || isPaused);
    ui->stopBtn->setEnabled(isRecording || isPaused);
    ui->selectAreaBtn->setEnabled(isIdle);
    ui->fullScreenRadio->setEnabled(isIdle);
    ui->areaRadio->setEnabled(isIdle);
    ui->fpsCombo->setEnabled(isIdle);
    ui->qualityCombo->setEnabled(isIdle);
    ui->audioCheck->setEnabled(isIdle);
    ui->screenCombo->setEnabled(isIdle);
    ui->outputPathEdit->setEnabled(isIdle);
    ui->browseBtn->setEnabled(isIdle);

    if (isRecording) {
        ui->pauseBtn->setText("暂停");
    } else if (isPaused) {
        ui->pauseBtn->setText("继续");
    } else {
        ui->pauseBtn->setText("暂停");
    }
}

void MainWindow::onStartClicked()
{
    QString outputPath = ui->outputPathEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        outputPath = QDir::homePath() + "/Videos";
    }

    QDir().mkpath(outputPath);
    outputPath += "/" + generateFileName();

    ScreenRecorder::RecordConfig config;
    config.fullScreen = ui->fullScreenRadio->isChecked();
    config.recordRect = m_selectedRect;
    config.frameRate = ui->fpsCombo->currentData().toInt();
    config.quality = ui->qualityCombo->currentData().toInt();
    config.recordAudio = ui->audioCheck->isChecked();
    config.outputPath = outputPath;
    config.screenIndex = ui->screenCombo->currentData().toInt();

    m_recorder->setConfig(config);
    m_recorder->startCountdown(3);
}

void MainWindow::onPauseClicked()
{
    if (m_recorder->state() == ScreenRecorder::Recording) {
        m_recorder->pauseRecording();
    } else if (m_recorder->state() == ScreenRecorder::Paused) {
        m_recorder->resumeRecording();
    }
}

void MainWindow::onStopClicked()
{
    m_recorder->stopRecording();
}

void MainWindow::onSelectAreaClicked()
{
    m_isSelectingArea = true;
    hide();

    QTimer::singleShot(300, this, [this]() {
        QList<QScreen*> screens = ScreenRecorder::getScreens();
        QScreen* selectedScreen = screens[ui->screenCombo->currentData().toInt()];

        QWidget* overlay = new QWidget();
        overlay->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        overlay->setAttribute(Qt::WA_TranslucentBackground);
        overlay->setGeometry(selectedScreen->geometry());
        overlay->show();

        QRubberBand* rubberBand = new QRubberBand(QRubberBand::Rectangle, overlay);

        QPoint startPos;
        bool selecting = false;

        overlay->installEventFilter(this);

        connect(overlay, &QWidget::mousePressEvent, [=](QMouseEvent* event) {
            startPos = event->pos();
            rubberBand->setGeometry(QRect(startPos, QSize()));
            rubberBand->show();
            selecting = true;
        });

        connect(overlay, &QWidget::mouseMoveEvent, [=](QMouseEvent* event) {
            if (selecting) {
                rubberBand->setGeometry(QRect(startPos, event->pos()).normalized());
            }
        });

        connect(overlay, &QWidget::mouseReleaseEvent, [=](QMouseEvent* event) {
            if (selecting) {
                m_selectedRect = QRect(startPos, event->pos()).normalized();
                rubberBand->hide();
                selecting = false;
                ui->areaRadio->setChecked(true);
                show();
                overlay->deleteLater();
            }
        });

        connect(overlay, &QWidget::keyPressEvent, [=](QKeyEvent* event) {
            if (event->key() == Qt::Key_Escape) {
                show();
                overlay->deleteLater();
            }
        });
    });
}

void MainWindow::onBrowseOutputPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择保存位置", ui->outputPathEdit->text());
    if (!dir.isEmpty()) {
        ui->outputPathEdit->setText(dir);
    }
}

void MainWindow::onOpenFolder()
{
    QString path = ui->outputPathEdit->text();
    if (path.isEmpty()) {
        path = QDir::homePath() + "/Videos";
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        show();
        raise();
        activateWindow();
    }
}

void MainWindow::onToggleRecording()
{
    if (m_recorder->state() == ScreenRecorder::Idle) {
        onStartClicked();
    } else {
        onStopClicked();
    }
}

void MainWindow::onStateChanged(ScreenRecorder::RecordState state)
{
    updateUI();

    if (state == ScreenRecorder::Recording) {
        showFloatingIndicator();
    } else if (state == ScreenRecorder::Idle) {
        hideFloatingIndicator();
    }
}

void MainWindow::onCountdownUpdated(int remaining)
{
    ui->statusLabel->setText(QString("倒计时: %1 秒").arg(remaining));
}

void MainWindow::onElapsedTimeUpdated(qint64 ms)
{
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    int hours = minutes / 60;
    QString timeStr = QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
    ui->statusLabel->setText("录制中: " + timeStr);

    if (m_floatingLabel) {
        m_floatingLabel->setText("● " + timeStr);
    }
}

void MainWindow::onRecordingFinished(const QString& filePath)
{
    ui->statusLabel->setText("录制完成");
    QMessageBox::information(this, "提示", QString("视频已保存至:\n%1").arg(filePath));
}

void MainWindow::onRecorderError(const QString& error)
{
    QMessageBox::critical(this, "错误", error);
}

void MainWindow::showFloatingIndicator()
{
    if (!m_floatingWidget) {
        m_floatingWidget = new QWidget();
        m_floatingWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        m_floatingWidget->setAttribute(Qt::WA_TranslucentBackground);

        QVBoxLayout* layout = new QVBoxLayout(m_floatingWidget);
        m_floatingLabel = new QLabel("● 00:00:00");
        m_floatingLabel->setStyleSheet("QLabel { color: red; font-size: 18px; font-weight: bold; background-color: rgba(0,0,0,150); padding: 5px; border-radius: 5px; }");
        layout->addWidget(m_floatingLabel);

        m_floatingWidget->adjustSize();
        m_floatingWidget->move(20, 20);
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
    ui->outputPathEdit->setText(settings.value("outputPath", QDir::homePath() + "/Videos").toString());
    ui->fpsCombo->setCurrentIndex(settings.value("fpsIndex", 1).toInt());
    ui->qualityCombo->setCurrentIndex(settings.value("qualityIndex", 1).toInt());
    ui->audioCheck->setChecked(settings.value("recordAudio", false).toBool());
}

void MainWindow::saveSettings()
{
    QSettings settings("ScreenRecorder", "Settings");
    settings.setValue("outputPath", ui->outputPathEdit->text());
    settings.setValue("fpsIndex", ui->fpsCombo->currentIndex());
    settings.setValue("qualityIndex", ui->qualityCombo->currentIndex());
    settings.setValue("recordAudio", ui->audioCheck->isChecked());
}

QString MainWindow::generateFileName()
{
    return QString("recording_%1.mp4").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_recorder->state() != ScreenRecorder::Idle) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "提示", "正在录制中，确定要退出吗？",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
        m_recorder->stopRecording();
    }

    saveSettings();
    event->accept();
}

