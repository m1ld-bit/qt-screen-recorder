#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <QSharedPointer>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class FFmpegEncoder : public QObject
{
    Q_OBJECT

public:
    enum QualityLevel {
        QualityLow,
        QualityMedium,
        QualityHigh
    };

    struct EncoderConfig {
        QString outputPath;
        int width;
        int height;
        int frameRate;
        int bitrate;
        QualityLevel quality;
        bool recordAudio;
        int audioSampleRate;
        int audioChannels;

        EncoderConfig()
            : width(1920)
            , height(1080)
            , frameRate(30)
            , bitrate(5000000)
            , quality(QualityMedium)
            , recordAudio(false)
            , audioSampleRate(44100)
            , audioChannels(2)
        {}
    };

    struct VideoFrame {
        QImage image;
        qint64 timestamp;
    };

    struct AudioFrame {
        QByteArray data;
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
    int getPendingAudioFrameCount() const;

signals:
    void errorOccurred(const QString& error);
    void encodingProgress(qint64 videoFrames, qint64 audioFrames);

private slots:
    void processVideoEncoding();
    void processAudioEncoding();
    void processMuxing();

private:
    bool initVideoEncoder();
    bool initAudioEncoder();
    bool initFormatContext();

    bool encodeVideoFrame(const VideoFrame& frame);
    bool encodeAudioFrame(const AudioFrame& frame);
    bool writePacket(AVPacket* packet);

    QImage convertToRGB32(const QImage& image);
    bool convertRGB32ToYUV420(const QImage& rgbImage, AVFrame* yuvFrame);

    static void freeAVFrame(AVFrame* frame);
    static void freeAVPacket(AVPacket* packet);

    EncoderConfig m_config;
    bool m_initialized;
    std::atomic<bool> m_running;
    std::atomic<bool> m_finished;

    // Video encoding
    AVFormatContext* m_formatContext;
    AVStream* m_videoStream;
    AVStream* m_audioStream;
    AVCodecContext* m_videoCodecContext;
    AVCodecContext* m_audioCodecContext;
    SwsContext* m_swsContext;
    SwrContext* m_swrContext;
    AVFrame* m_videoFrame;
    AVFrame* m_audioFrame;
    AVFrame* m_resampledAudioFrame;

    // Frame queues
    QQueue<QSharedPointer<VideoFrame>> m_videoQueue;
    QQueue<QSharedPointer<AudioFrame>> m_audioQueue;
    mutable QMutex m_videoQueueMutex;
    mutable QMutex m_audioQueueMutex;

    // Threads
    QThread* m_videoThread;
    QThread* m_audioThread;
    QThread* m_muxerThread;
    QWaitCondition m_videoWaitCondition;
    QWaitCondition m_audioWaitCondition;
    QWaitCondition m_muxerWaitCondition;

    // Statistics
    qint64 m_videoFrameCount;
    qint64 m_audioFrameCount;
    qint64 m_videoPts;
    qint64 m_audioPts;
};

#endif // FFMPEGENCODER_H
