#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QSharedPointer>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class FFmpegEncoder : public QObject
{
    Q_OBJECT

public:
    enum QualityLevel { QualityLow, QualityMedium, QualityHigh };

    struct EncoderConfig {
        QString outputPath;
        int width;
        int height;
        int frameRate;
        int bitrate = 5000000;
        QualityLevel quality = QualityMedium;
        bool recordAudio = false;
        int audioSampleRate = 44100;
        int audioChannels = 2;

        EncoderConfig() : width(1920), height(1080), frameRate(30) {}
    };

    struct VideoFrame {
        QImage image;
        qint64 timestamp;
    };

    explicit FFmpegEncoder(QObject* parent = nullptr);
    ~FFmpegEncoder() override;

    bool init(const EncoderConfig& config);
    bool isInitialized() const;
    
    void addVideoFrame(const QImage& image, qint64 timestamp);
    void addAudioFrame(const QByteArray& data, qint64 timestamp);

    bool finish();
    void cleanup();

    int getPendingVideoFrameCount() const;

signals:
    void errorOccurred(const QString& error);
    void encodingProgress(qint64 videoFrames, qint64 audioFrames);

private:
    bool initVideoStream();
    bool encodeImage(const QImage& image, qint64 timestamp);
    void processAllFrames();
    void flushVideoEncoder();

    EncoderConfig m_config;
    bool m_initialized = false;
    bool m_running = false;

    AVFormatContext* m_formatContext = nullptr;
    AVStream* m_videoStream = nullptr;
    AVCodecContext* m_videoCodecContext = nullptr;
    SwsContext* m_swsContext = nullptr;
    AVFrame* m_videoFrame = nullptr;

    QQueue<QSharedPointer<VideoFrame>> m_frameQueue;
    mutable QMutex m_queueMutex;

    qint64 m_videoFrameCount = 0;
    qint64 m_videoPts = 0;
};

#endif
