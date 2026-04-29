#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QMutex>

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
    struct EncoderConfig {
        QString outputPath;
        int width;
        int height;
        int frameRate;
        int bitrate;
        bool recordAudio;
        int audioSampleRate;
        int audioChannels;
    };

    explicit FFmpegEncoder(QObject* parent = nullptr);
    ~FFmpegEncoder();

    bool init(const EncoderConfig& config);
    bool encodeVideoFrame(const QImage& image, qint64 timestamp);
    bool encodeAudioFrame(const QByteArray& audioData, qint64 timestamp);
    bool finish();
    void cleanup();

    bool isInitialized() const { return m_initialized; }

signals:
    void errorOccurred(const QString& error);

private:
    bool initVideoStream();
    bool initAudioStream();
    bool openOutputFile();
    void writeTrailer();

    AVFormatContext* m_formatCtx;
    AVCodecContext* m_videoCodecCtx;
    AVCodecContext* m_audioCodecCtx;
    AVStream* m_videoStream;
    AVStream* m_audioStream;
    SwsContext* m_swsCtx;
    SwrContext* m_swrCtx;

    AVFrame* m_videoFrame;
    AVFrame* m_audioFrame;
    AVPacket* m_packet;

    EncoderConfig m_config;
    bool m_initialized;
    bool m_videoStarted;
    bool m_audioStarted;
    qint64 m_videoPts;
    qint64 m_audioPts;
    QMutex m_mutex;
};

#endif // FFMPEGENCODER_H

