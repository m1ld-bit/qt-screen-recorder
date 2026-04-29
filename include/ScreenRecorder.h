#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <QObject>
#include <QThread>
#include <QScreen>
#include <QRect>
#include <QTimer>
#include <QAudioInput>
#include <QAudioDeviceInfo>
#include "FFmpegEncoder.h"

class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    enum RecordState {
        Idle,
        CountingDown,
        Recording,
        Paused,
        Stopping
    };

    struct RecordConfig {
        bool fullScreen;
        QRect recordRect;
        int frameRate;
        int quality;
        bool recordAudio;
        QString outputPath;
        int screenIndex;
    };

    explicit ScreenRecorder(QObject* parent = nullptr);
    ~ScreenRecorder();

    void setConfig(const RecordConfig& config);
    void startCountdown(int seconds = 3);
    void startRecording();
    void pauseRecording();
    void resumeRecording();
    void stopRecording();

    RecordState state() const { return m_state; }
    qint64 elapsedTime() const { return m_elapsedTime; }

    static QList<QScreen*> getScreens();

signals:
    void stateChanged(RecordState state);
    void countdownUpdated(int remaining);
    void elapsedTimeUpdated(qint64 ms);
    void recordingFinished(const QString& filePath);
    void errorOccurred(const QString& error);

private slots:
    void onCountdownTick();
    void onCaptureTimer();
    void onAudioReadyRead();
    void onEncoderError(const QString& error);

private:
    void initEncoder();
    void startCapture();
    void stopCapture();
    int getBitrate(int quality) const;
    QImage captureScreen();

    RecordState m_state;
    RecordConfig m_config;
    FFmpegEncoder* m_encoder;
    QTimer* m_countdownTimer;
    QTimer* m_captureTimer;
    QThread* m_encoderThread;
    QAudioInput* m_audioInput;
    QIODevice* m_audioDevice;
    int m_countdownSeconds;
    qint64 m_elapsedTime;
    qint64 m_startTime;
    QScreen* m_selectedScreen;
};

#endif // SCREENRECORDER_H

