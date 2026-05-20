#include "FFmpegEncoder.h"
#include "Logger.h"
#include <QDebug>

FFmpegEncoder::FFmpegEncoder(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_running(false)
    , m_formatContext(nullptr)
    , m_videoStream(nullptr)
    , m_videoCodecContext(nullptr)
    , m_swsContext(nullptr)
    , m_videoFrame(nullptr)
    , m_videoFrameCount(0)
    , m_videoPts(0)
{
}

FFmpegEncoder::~FFmpegEncoder()
{
    cleanup();
}

bool FFmpegEncoder::init(const EncoderConfig& config)
{
    LOG_INFO(QString("Initializing FFmpegEncoder: %1x%2, %3 FPS")
             .arg(config.width).arg(config.height).arg(config.frameRate));

    if (m_initialized) {
        cleanup();
    }

    m_config = config;
    m_videoFrameCount = 0;
    m_videoPts = 0;

    int ret = avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr,
                                             config.outputPath.toUtf8().constData());
    if (!m_formatContext) {
        LOG_ERROR("Could not create output format context");
        return false;
    }

    if (!initVideoStream()) {
        LOG_ERROR("Failed to init video stream");
        cleanup();
        return false;
    }

    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_formatContext->pb, config.outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            LOG_ERROR(QString("Could not open output file: %1").arg(errBuf));
            cleanup();
            return false;
        }
    }

    ret = avformat_write_header(m_formatContext, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Error writing header: %1").arg(errBuf));
        cleanup();
        return false;
    }

    m_initialized = true;
    m_running = true;

    LOG_INFO("FFmpegEncoder initialized successfully");
    return true;
}

bool FFmpegEncoder::isInitialized() const
{
    return m_initialized;
}

bool FFmpegEncoder::initVideoStream()
{
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOG_ERROR("H.264 codec not found");
        return false;
    }

    m_videoCodecContext = avcodec_alloc_context3(codec);
    if (!m_videoCodecContext) {
        LOG_ERROR("Could not allocate video codec context");
        return false;
    }

    m_videoCodecContext->width = m_config.width;
    m_videoCodecContext->height = m_config.height;
    m_videoCodecContext->time_base = {1, 1000};
    m_videoCodecContext->framerate = {m_config.frameRate, 1};
    m_videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_videoCodecContext->gop_size = 12;
    m_videoCodecContext->max_b_frames = 0;
    m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(m_videoCodecContext->priv_data, "preset", "ultrafast", 0);

    int ret = avcodec_open2(m_videoCodecContext, codec, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Could not open video codec: %1").arg(errBuf));
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) {
        LOG_ERROR("Could not create video stream");
        return false;
    }

    ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecContext);
    if (ret < 0) {
        LOG_ERROR("Could not copy video parameters");
        return false;
    }

    m_videoStream->time_base = m_videoCodecContext->time_base;

    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = AV_PIX_FMT_YUV420P;
    m_videoFrame->width = m_config.width;
    m_videoFrame->height = m_config.height;
    ret = av_frame_get_buffer(m_videoFrame, 0);
    if (ret < 0) {
        LOG_ERROR("Could not allocate video frame buffer");
        return false;
    }

    m_swsContext = sws_getContext(
        m_config.width, m_config.height, AV_PIX_FMT_BGRA,
        m_config.width, m_config.height, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        LOG_ERROR("Could not create sws context");
        return false;
    }

    LOG_INFO("Video stream initialized");
    return true;
}

void FFmpegEncoder::addVideoFrame(const QImage& image, qint64 timestamp)
{
    if (!m_initialized || !m_running || image.isNull()) return;

    QMutexLocker locker(&m_queueMutex);
    auto frame = QSharedPointer<VideoFrame>::create();
    frame->image = image.convertToFormat(QImage::Format_ARGB32).copy();
    frame->timestamp = timestamp;
    m_frameQueue.enqueue(frame);
}

void FFmpegEncoder::addAudioFrame(const QByteArray& data, qint64 timestamp)
{
    Q_UNUSED(data)
    Q_UNUSED(timestamp)
}

bool FFmpegEncoder::encodeImage(const QImage& image, qint64 timestamp)
{
    if (!m_videoCodecContext || !m_videoFrame || !m_swsContext || image.isNull())
        return false;

    QImage img = image;
    if (img.size() != QSize(m_config.width, m_config.height)) {
        img = img.scaled(m_config.width, m_config.height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    const uint8_t* src[] = { img.constBits() };
    int srcStride[] = { static_cast<int>(img.bytesPerLine()) };

    sws_scale(m_swsContext, src, srcStride, 0, img.height(),
              m_videoFrame->data, m_videoFrame->linesize);

    m_videoFrame->pts = timestamp;

    int ret = avcodec_send_frame(m_videoCodecContext, m_videoFrame);
    if (ret < 0) return false;

    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_videoCodecContext, pkt) == 0) {
        av_packet_rescale_ts(pkt, m_videoCodecContext->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;
        pkt->duration = 1000 / m_config.frameRate;
        av_interleaved_write_frame(m_formatContext, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    m_videoFrameCount++;
    return true;
}

void FFmpegEncoder::processAllFrames()
{
    LOG_INFO("Processing all queued frames...");

    QMutexLocker locker(&m_queueMutex);
    int totalFrames = m_frameQueue.size();
    locker.unlock();

    LOG_INFO(QString("Found %1 frames to encode").arg(totalFrames));

    for (int i = 0; i < totalFrames; ++i) {
        QSharedPointer<VideoFrame> frame;
        {
            QMutexLocker lock(&m_queueMutex);
            if (m_frameQueue.isEmpty()) break;
            frame = m_frameQueue.dequeue();
        }

        if (frame && !frame->image.isNull()) {
            encodeImage(frame->image, frame->timestamp);
        }
        
        if ((i + 1) % 50 == 0 || i == totalFrames - 1) {
            int progress = (i + 1) * 100 / totalFrames;
            emit encodingProgress(m_videoFrameCount, 0);
            LOG_INFO(QString("Encoding progress: %1% (%2/%3 frames)")
                     .arg(progress).arg(i + 1).arg(totalFrames));
        }
    }

    flushVideoEncoder();
    LOG_INFO(QString("Encoding complete: %1 frames written").arg(m_videoFrameCount));
}

void FFmpegEncoder::flushVideoEncoder()
{
    if (!m_videoCodecContext) return;

    avcodec_send_frame(m_videoCodecContext, nullptr);

    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_videoCodecContext, pkt) == 0) {
        av_packet_rescale_ts(pkt, m_videoCodecContext->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;
        av_interleaved_write_frame(m_formatContext, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

bool FFmpegEncoder::finish()
{
    if (!m_initialized) return true;

    LOG_INFO("Finishing encoding...");
    m_running = false;

    processAllFrames();

    if (m_formatContext) {
        int ret = av_write_trailer(m_formatContext);
        if (ret == 0) {
            LOG_INFO("Trailer written - file should be valid!");
            
            if (m_videoStream && m_videoFrameCount > 0) {
                m_videoStream->duration = m_videoFrameCount * (1000 / m_config.frameRate);
                LOG_INFO(QString("Video duration set: %1 ms (%2 frames)")
                         .arg(m_videoStream->duration).arg(m_videoFrameCount));
            }
        } else {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            LOG_ERROR(QString("Trailer error: %1").arg(errBuf));
        }
    }

    LOG_INFO(QString("Done: %1 video frames encoded").arg(m_videoFrameCount));

    cleanup();
    return true;
}

void FFmpegEncoder::cleanup()
{
    m_running = false;
    m_initialized = false;

    av_frame_free(&m_videoFrame);
    avcodec_free_context(&m_videoCodecContext);
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;

    if (m_formatContext) {
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_formatContext->pb);
        }
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }

    QMutexLocker lock(&m_queueMutex);
    m_frameQueue.clear();

    LOG_INFO("Cleanup done");
}

int FFmpegEncoder::getPendingVideoFrameCount() const
{
    QMutexLocker lock(&m_queueMutex);
    return m_frameQueue.size();
}
