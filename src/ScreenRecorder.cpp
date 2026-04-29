#include "ScreenRecorder.h"
#include "Logger.h"
#include <QGuiApplication>
#include <QDateTime>
#include <QBuffer>

ScreenRecorder::ScreenRecorder(QObject* parent)
    : QObject(parent)
    , m_state(Idle)
    , m_encoder(nullptr)
    , m_countdownTimer(nullptr)
    , m_captureTimer(nullptr)
    , m_encoderThread(nullptr)
    , m_audioInput(nullptr)
    , m_audioDevice(nullptr)
    , m_countdownSeconds(0)
    , m_elapsedTime(0)
    , m_startTime(0)
    , m_selectedScreen(nullptr)
{
    m_encoder = new FFmpegEncoder();
    m_encoderThread = new QThread();
    m_encoder->moveToThread(m_encoderThread);
    m_encoderThread->start();

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &ScreenRecorder::onCountdownTick);

    m_captureTimer = new QTimer(this);
    connect(m_captureTimer, &QTimer::timeout, this, &ScreenRecorder::onCaptureTimer);

    connect(m_encoder, &FFmpegEncoder::errorOccurred, this, &ScreenRecorder::onEncoderError);
}

ScreenRecorder::~ScreenRecorder()
{
    if (m_state != Idle) {
        stopRecording();
    }

    if (m_encoderThread) {
        m_encoderThread->quit();
        m_encoderThread->wait();
        delete m_encoderThread;
    }

    if (m_encoder) {
        delete m_encoder;
    }
}

QList<QScreen*> ScreenRecorder::getScreens()
{
    return QGuiApplication::screens();
}

void ScreenRecorder::setConfig(const RecordConfig& config)
{
    m_config = config;
}

void ScreenRecorder::startCountdown(int seconds)
{
    if (m_state != Idle) {
        return;
    }

    m_countdownSeconds = seconds;
    m_state = CountingDown;
    emit stateChanged(m_state);
    emit countdownUpdated(m_countdownSeconds);
    m_countdownTimer->start();

    LOG_INFO(QString("Countdown started: %1 seconds").arg(seconds));
}

void ScreenRecorder::onCountdownTick()
{
    m_countdownSeconds--;
    emit countdownUpdated(m_countdownSeconds);

    if (m_countdownSeconds <= 0) {
        m_countdownTimer->stop();
        startRecording();
    }
}

void ScreenRecorder::startRecording()
{
    if (m_state != Idle && m_state != CountingDown) {
        return;
    }

    QList<QScreen*> screens = getScreens();
    if (m_config.screenIndex >= 0 && m_config.screenIndex < screens.size()) {
        m_selectedScreen = screens[m_config.screenIndex];
    } else {
        m_selectedScreen = QGuiApplication::primaryScreen();
    }

    if (m_config.fullScreen) {
        m_config.recordRect = m_selectedScreen->geometry();
    }

    initEncoder();

    m_state = Recording;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_elapsedTime = 0;
    emit stateChanged(m_state);

    startCapture();

    LOG_INFO("Recording started");
}

void ScreenRecorder::initEncoder()
{
    FFmpegEncoder::EncoderConfig encoderConfig;
    encoderConfig.outputPath = m_config.outputPath;
    encoderConfig.width = m_config.recordRect.width();
    encoderConfig.height = m_config.recordRect.height();
    encoderConfig.frameRate = m_config.frameRate;
    encoderConfig.bitrate = getBitrate(m_config.quality);
    encoderConfig.recordAudio = m_config.recordAudio;
    encoderConfig.audioSampleRate = 44100;
    encoderConfig.audioChannels = 2;

    m_encoder->init(encoderConfig);
}

int ScreenRecorder::getBitrate(int quality) const
{
    int width = m_config.recordRect.width();
    int height = m_config.recordRect.height();
    int baseBitrate = width * height * m_config.frameRate / 100;

    switch (quality) {
        case 0: return baseBitrate * 0.5;
        case 1: return baseBitrate;
        case 2: return baseBitrate * 2;
        default: return baseBitrate;
    }
}

void ScreenRecorder::startCapture()
{
    int interval = 1000 / m_config.frameRate;
    m_captureTimer->start(interval);

    if (m_config.recordAudio) {
        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(2);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(format)) {
            format = info.nearestFormat(format);
        }

        m_audioInput = new QAudioInput(format, this);
        m_audioDevice = m_audioInput->start();
        connect(m_audioDevice, &QIODevice::readyRead, this, &ScreenRecorder::onAudioReadyRead);
    }
}

void ScreenRecorder::stopCapture()
{
    m_captureTimer->stop();

    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }
}

void ScreenRecorder::pauseRecording()
{
    if (m_state != Recording) {
        return;
    }

    m_state = Paused;
    stopCapture();
    emit stateChanged(m_state);

    LOG_INFO("Recording paused");
}

void ScreenRecorder::resumeRecording()
{
    if (m_state != Paused) {
        return;
    }

    m_state = Recording;
    m_startTime = QDateTime::currentMSecsSinceEpoch() - m_elapsedTime;
    startCapture();
    emit stateChanged(m_state);

    LOG_INFO("Recording resumed");
}

void ScreenRecorder::stopRecording()
{
    if (m_state == Idle || m_state == Stopping) {
        return;
    }

    m_state = Stopping;
    emit stateChanged(m_state);

    stopCapture();

    if (m_encoder->isInitialized()) {
        m_encoder->finish();
    }

    m_state = Idle;
    emit stateChanged(m_state);
    emit recordingFinished(m_config.outputPath);

    LOG_INFO("Recording stopped");
}

void ScreenRecorder::onCaptureTimer()
{
    if (m_state != Recording) {
        return;
    }

    m_elapsedTime = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    emit elapsedTimeUpdated(m_elapsedTime);

    QImage frame = captureScreen();
    if (!frame.isNull()) {
        m_encoder->encodeVideoFrame(frame, m_elapsedTime);
    }
}

QImage ScreenRecorder::captureScreen()
{
    if (!m_selectedScreen) {
        return QImage();
    }

    QRect rect = m_config.recordRect;
    QPoint screenTopLeft = m_selectedScreen->geometry().topLeft();
    QRect captureRect(
        rect.left() - screenTopLeft.x(),
        rect.top() - screenTopLeft.y(),
        rect.width(),
        rect.height()
    );

    return m_selectedScreen->grabWindow(0, captureRect.x(), captureRect.y(),
                                        captureRect.width(), captureRect.height()).toImage();
}

void ScreenRecorder::onAudioReadyRead()
{
    if (m_state != Recording || !m_audioDevice) {
        return;
    }

    QByteArray data = m_audioDevice->readAll();
    m_encoder->encodeAudioFrame(data, m_elapsedTime);
}

void ScreenRecorder::onEncoderError(const QString& error)
{
    LOG_ERROR(QString("Encoder error: %1").arg(error));
    emit errorOccurred(error);
}

