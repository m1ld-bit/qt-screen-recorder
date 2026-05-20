#include "ScreenRecorder.h"
#include "Logger.h"
#include <QGuiApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QPixmap>

ScreenRecorder::ScreenRecorder(QObject* parent)
    : QObject(parent)
    , m_state(Idle)
    , m_selectedScreen(nullptr)
    , m_captureThread(nullptr)
    , m_captureTimer(nullptr)
    , m_startTime(0)
    , m_screenMonitorTimer(nullptr)
    , m_lastScreenCount(0)
{
    LOG_INFO("ScreenRecorder initialized");

    // 创建屏幕监控定时器，每2秒检查一次屏幕配置变化
    m_screenMonitorTimer = new QTimer(this);
    m_screenMonitorTimer->setInterval(2000);
    connect(m_screenMonitorTimer, &QTimer::timeout, this, &ScreenRecorder::onScreenConfigChanged);
}

ScreenRecorder::~ScreenRecorder()
{
    LOG_INFO("ScreenRecorder destroying");
    
    // 确保停止采集
    if (m_state.loadAcquire() != Idle) {
        stop();
    }
    
    // 清理资源
    cleanup();
}

QList<QScreen*> ScreenRecorder::getScreens()
{
    return QGuiApplication::screens();
}

QScreen* ScreenRecorder::getPrimaryScreen()
{
    return QGuiApplication::primaryScreen();
}

void ScreenRecorder::setConfig(const CaptureConfig& config)
{
    // 只能在空闲状态下修改配置
    if (m_state.loadAcquire() != Idle) {
        LOG_WARN("Cannot change config while recording");
        return;
    }

    m_config = config;
    LOG_INFO(QString("Capture config updated: fullScreen=%1, fps=%2, screenIndex=%3")
             .arg(config.fullScreen)
             .arg(config.frameRate)
             .arg(config.screenIndex));
}

ScreenRecorder::CaptureConfig ScreenRecorder::config() const
{
    return m_config;
}

bool ScreenRecorder::start()
{
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState != Idle) {
        LOG_WARN("Cannot start recording: not in idle state");
        emit errorOccurred("Cannot start: recorder not in idle state");
        return false;
    }

    LOG_INFO("Starting screen capture...");

    // 初始化采集参数
    if (!initializeCapture()) {
        emit errorOccurred("Failed to initialize capture");
        return false;
    }

    m_captureTimer = new QTimer(this);
    m_captureTimer->setTimerType(Qt::PreciseTimer);
    connect(m_captureTimer, &QTimer::timeout, this, &ScreenRecorder::onCaptureTick);
    
    int interval = 1000 / m_config.frameRate;
    m_captureTimer->start(interval);
    LOG_INFO(QString("Capture timer started with interval: %1 ms").arg(interval));
    
    // 记录开始时间
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    
    // 启动屏幕监控
    m_screenMonitorTimer->start();
    
    // 更新状态
    setState(Recording);
    
    LOG_INFO("Screen capture started successfully");
    return true;
}

void ScreenRecorder::pause()
{
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState != Recording) {
        LOG_WARN("Cannot pause: not in recording state");
        return;
    }

    if (m_captureTimer && m_captureTimer->isActive()) {
        m_captureTimer->stop();
    }

    setState(Paused);
    LOG_INFO("Screen capture paused");
}

void ScreenRecorder::resume()
{
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState != Paused) {
        LOG_WARN("Cannot resume: not in paused state");
        return;
    }

    if (m_captureTimer) {
        int interval = 1000 / m_config.frameRate;
        m_captureTimer->start(interval);
    }

    setState(Recording);
    LOG_INFO("Screen capture resumed");
}

void ScreenRecorder::stop()
{
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState == Idle) {
        LOG_INFO("Already idle");
        return;
    }

    LOG_INFO("Stopping screen capture...");

    // 停止屏幕监控
    m_screenMonitorTimer->stop();

    // 停止采集定时器
    if (m_captureTimer) {
        m_captureTimer->stop();
    }

    // 清理资源
    cleanup();

    // 更新状态
    setState(Idle);

    LOG_INFO("Screen capture stopped");
}

ScreenRecorder::RecordState ScreenRecorder::state() const
{
    return static_cast<RecordState>(m_state.loadAcquire());
}

void ScreenRecorder::setFrameRate(int fps)
{
    if (fps <= 0) {
        LOG_WARN(QString("Invalid frame rate: %1").arg(fps));
        return;
    }

    // 更新配置
    m_config.frameRate = fps;

    // 如果正在录制，实时更新定时器
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState == Recording && m_captureTimer) {
        int interval = 1000 / fps;
        
        // 使用QMetaObject::invokeMethod确保在定时器所在线程中执行
        QMetaObject::invokeMethod(m_captureTimer, [this, interval]() {
            m_captureTimer->setInterval(interval);
            m_captureTimer->start();
        }, Qt::QueuedConnection);
        
        LOG_INFO(QString("Frame rate changed to %1 FPS").arg(fps));
    }
}

int ScreenRecorder::frameRate() const
{
    return m_config.frameRate;
}

QSharedPointer<ScreenRecorder::Frame> ScreenRecorder::getFrame()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_frameQueue.isEmpty()) {
        return nullptr;
    }
    
    return m_frameQueue.dequeue();
}

int ScreenRecorder::queueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_frameQueue.size();
}

void ScreenRecorder::clearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    m_frameQueue.clear();
    LOG_INFO("Frame queue cleared");
}

void ScreenRecorder::onCaptureTick()
{
    RecordState currentState = static_cast<RecordState>(m_state.loadAcquire());
    if (currentState != Recording) {
        return;
    }

    // 采集一帧
    QImage frameImage = captureFrame();
    if (frameImage.isNull()) {
        LOG_WARN("Failed to capture frame");
        return;
    }

    // 计算时间戳
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch() - m_startTime;

    // 创建帧对象（智能指针管理）
    auto frame = QSharedPointer<Frame>::create(frameImage, timestamp);

    // 添加到队列（线程安全）
    {
        QMutexLocker locker(&m_queueMutex);
        
        m_frameQueue.enqueue(frame);
    }

    // 发送新帧可用信号
    emit newFrameAvailable(frame);
}

void ScreenRecorder::onScreenConfigChanged()
{
    bool changed = false;
    
    // 检查显示器数量变化
    int currentScreenCount = QGuiApplication::screens().size();
    if (currentScreenCount != m_lastScreenCount) {
        changed = true;
        m_lastScreenCount = currentScreenCount;
        LOG_INFO(QString("Screen count changed: %1 -> %2").arg(m_lastScreenCount).arg(currentScreenCount));
    }
    
    // 检查选中显示器的几何变化
    if (m_selectedScreen) {
        QRect currentGeometry = m_selectedScreen->geometry();
        if (currentGeometry != m_lastScreenGeometry) {
            changed = true;
            m_lastScreenGeometry = currentGeometry;
            LOG_INFO(QString("Screen geometry changed: %1x%2 -> %3x%4")
                     .arg(m_lastScreenGeometry.width()).arg(m_lastScreenGeometry.height())
                     .arg(currentGeometry.width()).arg(currentGeometry.height()));
            
            // 如果是全屏录制，自动更新录制区域
            if (m_config.fullScreen) {
                m_config.recordRect = currentGeometry;
                LOG_INFO("Auto-updated fullscreen record rect to new screen geometry");
            }
        }
    }
    
    if (changed) {
        emit screenConfigChanged();
    }
}

bool ScreenRecorder::initializeCapture()
{
    // 更新选择的显示器
    updateSelectedScreen();
    
    if (!m_selectedScreen) {
        LOG_ERROR("No screen available");
        return false;
    }
    
    // 如果是全屏，自动设置录制区域
    if (m_config.fullScreen) {
        m_config.recordRect = m_selectedScreen->geometry();
    }
    
    // 验证录制区域
    if (m_config.recordRect.isEmpty()) {
        LOG_ERROR("Invalid record rect: empty");
        return false;
    }
    
    // 记录初始屏幕状态
    m_lastScreenGeometry = m_selectedScreen->geometry();
    m_lastScreenCount = QGuiApplication::screens().size();
    
    LOG_INFO(QString("Capture initialized: screen=%1x%2, rect=(%3,%4,%5,%6)")
             .arg(m_config.recordRect.width())
             .arg(m_config.recordRect.height())
             .arg(m_config.recordRect.x())
             .arg(m_config.recordRect.y())
             .arg(m_config.recordRect.width())
             .arg(m_config.recordRect.height()));
    
    return true;
}

void ScreenRecorder::cleanup()
{
    // 清理采集定时器
    if (m_captureTimer) {
        m_captureTimer->deleteLater();
        m_captureTimer = nullptr;
    }
    
    // 清空帧队列
    clearQueue();
    
    m_selectedScreen = nullptr;
}

QImage ScreenRecorder::captureFrame()
{
    if (!m_selectedScreen) {
        return QImage();
    }
    
    // 计算相对于显示器的采集区域
    QRect screenGeometry = m_selectedScreen->geometry();
    QRect captureRect(
        m_config.recordRect.x() - screenGeometry.x(),
        m_config.recordRect.y() - screenGeometry.y(),
        m_config.recordRect.width(),
        m_config.recordRect.height()
    );
    
    // 捕获屏幕
    return m_selectedScreen->grabWindow(0, captureRect.x(), captureRect.y(),
                                      captureRect.width(), captureRect.height()).toImage();
}

void ScreenRecorder::updateSelectedScreen()
{
    QList<QScreen*> screens = QGuiApplication::screens();
    
    if (m_config.screenIndex >= 0 && m_config.screenIndex < screens.size()) {
        m_selectedScreen = screens[m_config.screenIndex];
    } else {
        m_selectedScreen = QGuiApplication::primaryScreen();
    }
    
    if (m_selectedScreen) {
        LOG_INFO(QString("Selected screen: index=%1, geometry=%2x%3")
                 .arg(m_config.screenIndex)
                 .arg(m_selectedScreen->geometry().width())
                 .arg(m_selectedScreen->geometry().height()));
    }
}

void ScreenRecorder::setState(RecordState newState)
{
    RecordState oldState = static_cast<RecordState>(m_state.loadAcquire());
    if (oldState == newState) {
        return;
    }
    
    m_state.storeRelease(newState);
    emit stateChanged(newState);
    
    LOG_INFO(QString("State changed: %1 -> %2")
             .arg(oldState == Idle ? "Idle" : oldState == Recording ? "Recording" : "Paused")
             .arg(newState == Idle ? "Idle" : newState == Recording ? "Recording" : "Paused"));
}
