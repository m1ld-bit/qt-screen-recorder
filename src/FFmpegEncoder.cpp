#include "FFmpegEncoder.h"
#include "Logger.h"
#include <QDebug>
#include <QElapsedTimer>

FFmpegEncoder::FFmpegEncoder(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_running(false)
    , m_finished(false)
    , m_formatContext(nullptr)
    , m_videoStream(nullptr)
    , m_audioStream(nullptr)
    , m_videoCodecContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_swsContext(nullptr)
    , m_swrContext(nullptr)
    , m_videoFrame(nullptr)
    , m_audioFrame(nullptr)
    , m_resampledAudioFrame(nullptr)
    , m_videoThread(nullptr)
    , m_audioThread(nullptr)
    , m_muxerThread(nullptr)
    , m_videoFrameCount(0)
    , m_audioFrameCount(0)
    , m_videoPts(0)
    , m_audioPts(0)
{
    LOG_INFO("FFmpegEncoder created");
}

FFmpegEncoder::~FFmpegEncoder()
{
    LOG_INFO("FFmpegEncoder destroying...");
    cleanup();
    LOG_INFO("FFmpegEncoder destroyed");
}

bool FFmpegEncoder::init(const EncoderConfig& config)
{
    LOG_INFO(QString("Initializing FFmpegEncoder: %1x%2, %3 FPS, bitrate: %4")
             .arg(config.width).arg(config.height).arg(config.frameRate).arg(config.bitrate));

    if (m_initialized) {
        LOG_WARN("Encoder already initialized, cleaning up first");
        cleanup();
    }

    m_config = config;
    m_videoFrameCount = 0;
    m_audioFrameCount = 0;
    m_videoPts = 0;
    m_audioPts = 0;
    m_finished = false;

    // Initialize format context
    if (!initFormatContext()) {
        LOG_ERROR("Failed to init format context");
        return false;
    }

    // Initialize video encoder
    if (!initVideoEncoder()) {
        LOG_ERROR("Failed to init video encoder");
        cleanup();
        return false;
    }

    // Initialize audio encoder if needed
    if (m_config.recordAudio) {
        if (!initAudioEncoder()) {
            LOG_ERROR("Failed to init audio encoder");
            cleanup();
            return false;
        }
    }

    // Open output file
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&m_formatContext->pb, m_config.outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            LOG_ERROR(QString("Could not open output file: %1").arg(errBuf));
            cleanup();
            return false;
        }
    }

    // Write file header
    int ret = avformat_write_header(m_formatContext, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Error writing file header: %1").arg(errBuf));
        cleanup();
        return false;
    }

    m_initialized = true;
    m_running = true;

    // Start encoding threads
    // Video thread
    m_videoThread = QThread::create([this]() { processVideoEncoding(); });
    m_videoThread->start();

    // Audio thread if needed
    if (m_config.recordAudio) {
        m_audioThread = QThread::create([this]() { processAudioEncoding(); });
        m_audioThread->start();
    }

    // Muxer thread
    m_muxerThread = QThread::create([this]() { processMuxing(); });
    m_muxerThread->start();

    LOG_INFO("FFmpegEncoder initialized successfully");
    return true;
}

bool FFmpegEncoder::isInitialized() const
{
    return m_initialized;
}

bool FFmpegEncoder::initFormatContext()
{
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, m_config.outputPath.toUtf8().constData());
    if (!m_formatContext) {
        LOG_ERROR("Could not create output format context");
        return false;
    }

    return true;
}

bool FFmpegEncoder::initVideoEncoder()
{
    const AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec) {
        videoCodec = avcodec_find_encoder_by_name("libx264");
    }
    if (!videoCodec) {
        LOG_ERROR("H.264 codec not found");
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) {
        LOG_ERROR("Could not create video stream");
        return false;
    }

    m_videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (!m_videoCodecContext) {
        LOG_ERROR("Could not allocate video codec context");
        return false;
    }

    m_videoCodecContext->codec_id = AV_CODEC_ID_H264;
    m_videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    m_videoCodecContext->width = m_config.width;
    m_videoCodecContext->height = m_config.height;
    m_videoCodecContext->time_base = AVRational{1, m_config.frameRate};
    m_videoCodecContext->framerate = AVRational{m_config.frameRate, 1};
    m_videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_videoCodecContext->gop_size = 30;
    m_videoCodecContext->max_b_frames = 2;

    // Set bitrate based on quality
    switch (m_config.quality) {
        case QualityLow:
            m_videoCodecContext->bit_rate = m_config.width * m_config.height * 2;
            av_opt_set(m_videoCodecContext->priv_data, "preset", "ultrafast", 0);
            av_opt_set(m_videoCodecContext->priv_data, "crf", "30", 0);
            break;
        case QualityMedium:
            m_videoCodecContext->bit_rate = m_config.width * m_config.height * 4;
            av_opt_set(m_videoCodecContext->priv_data, "preset", "fast", 0);
            av_opt_set(m_videoCodecContext->priv_data, "crf", "23", 0);
            break;
        case QualityHigh:
            m_videoCodecContext->bit_rate = m_config.width * m_config.height * 8;
            av_opt_set(m_videoCodecContext->priv_data, "preset", "medium", 0);
            av_opt_set(m_videoCodecContext->priv_data, "crf", "18", 0);
            break;
    }

    // Override with user-specified bitrate if provided
    if (m_config.bitrate > 0) {
        m_videoCodecContext->bit_rate = m_config.bitrate;
    }

    // Copy stream parameters
    int ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecContext);
    if (ret < 0) {
        LOG_ERROR("Could not copy video parameters");
        return false;
    }

    // Open codec
    ret = avcodec_open2(m_videoCodecContext, videoCodec, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Could not open video codec: %1").arg(errBuf));
        return false;
    }

    // Allocate video frame
    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = m_videoCodecContext->pix_fmt;
    m_videoFrame->width = m_videoCodecContext->width;
    m_videoFrame->height = m_videoCodecContext->height;
    ret = av_frame_get_buffer(m_videoFrame, 32);
    if (ret < 0) {
        LOG_ERROR("Could not allocate video frame buffer");
        return false;
    }

    // Initialize sws context for RGB32 -> YUV420P conversion
    m_swsContext = sws_getContext(
        m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_RGBA,
        m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!m_swsContext) {
        LOG_ERROR("Could not initialize sws context");
        return false;
    }

    m_videoStream->time_base = m_videoCodecContext->time_base;
    LOG_INFO("Video encoder initialized");
    return true;
}

bool FFmpegEncoder::initAudioEncoder()
{
    const AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec) {
        LOG_ERROR("AAC codec not found");
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) {
        LOG_ERROR("Could not create audio stream");
        return false;
    }

    m_audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (!m_audioCodecContext) {
        LOG_ERROR("Could not allocate audio codec context");
        return false;
    }

    m_audioCodecContext->codec_id = AV_CODEC_ID_AAC;
    m_audioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioCodecContext->sample_rate = m_config.audioSampleRate;
    m_audioCodecContext->ch_layout.nb_channels = m_config.audioChannels;
    m_audioCodecContext->ch_layout.order = AV_CHANNEL_ORDER_NATIVE;
    m_audioCodecContext->ch_layout.u.mask = (m_config.audioChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    m_audioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    m_audioCodecContext->time_base = AVRational{1, m_config.audioSampleRate};
    m_audioCodecContext->bit_rate = 128000;

    // Copy stream parameters
    int ret = avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecContext);
    if (ret < 0) {
        LOG_ERROR("Could not copy audio parameters");
        return false;
    }

    // Open codec
    ret = avcodec_open2(m_audioCodecContext, audioCodec, nullptr);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Could not open audio codec: %1").arg(errBuf));
        return false;
    }

    // Allocate audio frame
    m_audioFrame = av_frame_alloc();
    m_audioFrame->nb_samples = m_audioCodecContext->frame_size;
    m_audioFrame->format = m_audioCodecContext->sample_fmt;
    av_channel_layout_copy(&m_audioFrame->ch_layout, &m_audioCodecContext->ch_layout);
    ret = av_frame_get_buffer(m_audioFrame, 0);
    if (ret < 0) {
        LOG_ERROR("Could not allocate audio frame buffer");
        return false;
    }

    // Allocate resampled audio frame
    m_resampledAudioFrame = av_frame_alloc();

    // Initialize swr context for audio resampling
    m_swrContext = swr_alloc();
    av_opt_set_chlayout(m_swrContext, "in_chlayout", &m_audioCodecContext->ch_layout, 0);
    av_opt_set_int(m_swrContext, "in_sample_rate", m_config.audioSampleRate, 0);
    av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_chlayout(m_swrContext, "out_chlayout", &m_audioCodecContext->ch_layout, 0);
    av_opt_set_int(m_swrContext, "out_sample_rate", m_config.audioSampleRate, 0);
    av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", m_audioCodecContext->sample_fmt, 0);
    ret = swr_init(m_swrContext);
    if (ret < 0) {
        LOG_ERROR("Could not initialize swr context");
        return false;
    }

    m_audioStream->time_base = m_audioCodecContext->time_base;
    LOG_INFO("Audio encoder initialized");
    return true;
}

void FFmpegEncoder::addVideoFrame(const QImage& image, qint64 timestamp)
{
    if (!m_initialized || !m_running) {
        return;
    }

    QMutexLocker locker(&m_videoQueueMutex);
    auto frame = QSharedPointer<VideoFrame>::create();
    frame->image = image;
    frame->timestamp = timestamp;
    m_videoQueue.enqueue(frame);
    m_videoWaitCondition.wakeOne();
}

void FFmpegEncoder::addAudioFrame(const QByteArray& data, qint64 timestamp)
{
    if (!m_initialized || !m_running || !m_config.recordAudio) {
        return;
    }

    QMutexLocker locker(&m_audioQueueMutex);
    auto frame = QSharedPointer<AudioFrame>::create();
    frame->data = data;
    frame->timestamp = timestamp;
    m_audioQueue.enqueue(frame);
    m_audioWaitCondition.wakeOne();
}

void FFmpegEncoder::processVideoEncoding()
{
    LOG_INFO("Video encoding thread started");

    while (m_running || !m_videoQueue.empty()) {
        QSharedPointer<VideoFrame> frame;

        {
            QMutexLocker locker(&m_videoQueueMutex);
            if (m_videoQueue.isEmpty()) {
                m_videoWaitCondition.wait(&m_videoQueueMutex, 100);
                continue;
            }
            frame = m_videoQueue.dequeue();
        }

        if (frame) {
            encodeVideoFrame(*frame);
        }
    }

    // Flush remaining frames
    if (m_videoCodecContext && m_initialized) {
        encodeVideoFrame(VideoFrame());
    }

    LOG_INFO("Video encoding thread finished");
}

void FFmpegEncoder::processAudioEncoding()
{
    if (!m_config.recordAudio) {
        return;
    }

    LOG_INFO("Audio encoding thread started");

    while (m_running || !m_audioQueue.empty()) {
        QSharedPointer<AudioFrame> frame;

        {
            QMutexLocker locker(&m_audioQueueMutex);
            if (m_audioQueue.isEmpty()) {
                m_audioWaitCondition.wait(&m_audioQueueMutex, 100);
                continue;
            }
            frame = m_audioQueue.dequeue();
        }

        if (frame) {
            encodeAudioFrame(*frame);
        }
    }

    // Flush remaining frames
    if (m_audioCodecContext && m_initialized) {
        encodeAudioFrame(AudioFrame());
    }

    LOG_INFO("Audio encoding thread finished");
}

void FFmpegEncoder::processMuxing()
{
    LOG_INFO("Muxing thread started");
    // Muxing is handled in the encodeVideoFrame and encodeAudioFrame functions
    // This thread just waits for the finish signal
    while (m_running || !m_finished) {
        QThread::msleep(10);
    }
    LOG_INFO("Muxing thread finished");
}

bool FFmpegEncoder::encodeVideoFrame(const VideoFrame& frame)
{
    if (!m_initialized || !m_videoCodecContext) {
        return false;
    }

    // If frame is empty, it's a flush request
    if (frame.image.isNull()) {
        return avcodec_send_frame(m_videoCodecContext, nullptr) >= 0;
    }

    // Convert QImage to YUV420P
    if (!convertRGB32ToYUV420(frame.image, m_videoFrame)) {
        LOG_ERROR("Failed to convert RGB32 to YUV420");
        return false;
    }

    m_videoFrame->pts = m_videoPts++;

    int ret = avcodec_send_frame(m_videoCodecContext, m_videoFrame);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Error sending video frame: %1").arg(errBuf));
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_videoCodecContext, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            LOG_ERROR(QString("Error encoding video: %1").arg(errBuf));
            av_packet_free(&pkt);
            return false;
        }

        av_packet_rescale_ts(pkt, m_videoCodecContext->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;

        if (!writePacket(pkt)) {
            av_packet_free(&pkt);
            return false;
        }

        av_packet_free(&pkt);
    }

    m_videoFrameCount++;
    emit encodingProgress(m_videoFrameCount, m_audioFrameCount);
    return true;
}

bool FFmpegEncoder::encodeAudioFrame(const AudioFrame& frame)
{
    if (!m_initialized || !m_audioCodecContext || !m_config.recordAudio) {
        return false;
    }

    // If frame is empty, it's a flush request
    if (frame.data.isEmpty()) {
        return avcodec_send_frame(m_audioCodecContext, nullptr) >= 0;
    }

    // Process audio data (audio encoding is simplified for now)
    int ret = avcodec_send_frame(m_audioCodecContext, m_audioFrame);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Error sending audio frame: %1").arg(errBuf));
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_audioCodecContext, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, sizeof(errBuf));
            LOG_ERROR(QString("Error encoding audio: %1").arg(errBuf));
            av_packet_free(&pkt);
            return false;
        }

        av_packet_rescale_ts(pkt, m_audioCodecContext->time_base, m_audioStream->time_base);
        pkt->stream_index = m_audioStream->index;

        if (!writePacket(pkt)) {
            av_packet_free(&pkt);
            return false;
        }

        av_packet_free(&pkt);
    }

    m_audioFrameCount++;
    emit encodingProgress(m_videoFrameCount, m_audioFrameCount);
    return true;
}

bool FFmpegEncoder::writePacket(AVPacket* packet)
{
    if (!m_initialized || !m_formatContext) {
        return false;
    }

    int ret = av_interleaved_write_frame(m_formatContext, packet);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        LOG_ERROR(QString("Error writing packet: %1").arg(errBuf));
        return false;
    }

    return true;
}

QImage FFmpegEncoder::convertToRGB32(const QImage& image)
{
    if (image.format() == QImage::Format_RGBA8888 || image.format() == QImage::Format_RGB32) {
        return image;
    }
    return image.convertToFormat(QImage::Format_RGB32);
}

bool FFmpegEncoder::convertRGB32ToYUV420(const QImage& rgbImage, AVFrame* yuvFrame)
{
    if (!yuvFrame || !m_swsContext) {
        return false;
    }

    QImage rgb32 = convertToRGB32(rgbImage);
    if (rgb32.width() != m_config.width || rgb32.height() != m_config.height) {
        rgb32 = rgb32.scaled(m_config.width, m_config.height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    const uint8_t* srcData[4] = {rgb32.bits(), nullptr, nullptr, nullptr};
    int srcLinesize[4] = {static_cast<int>(rgb32.bytesPerLine()), 0, 0, 0};

    sws_scale(m_swsContext, srcData, srcLinesize, 0, rgb32.height(),
              yuvFrame->data, yuvFrame->linesize);

    return true;
}

bool FFmpegEncoder::finish()
{
    if (!m_initialized) {
        return true;
    }

    LOG_INFO("Finishing encoding...");
    m_running = false;
    m_finished = true;

    // Wake up all threads
    m_videoWaitCondition.wakeAll();
    m_audioWaitCondition.wakeAll();

    // Wait for threads to finish
    if (m_videoThread) {
        m_videoThread->wait();
        m_videoThread->deleteLater();
        m_videoThread = nullptr;
    }
    if (m_audioThread) {
        m_audioThread->wait();
        m_audioThread->deleteLater();
        m_audioThread = nullptr;
    }
    if (m_muxerThread) {
        m_muxerThread->wait();
        m_muxerThread->deleteLater();
        m_muxerThread = nullptr;
    }

    // Write file trailer
    if (m_formatContext) {
        av_write_trailer(m_formatContext);
    }

    LOG_INFO(QString("Encoding finished: %1 video frames, %2 audio frames")
             .arg(m_videoFrameCount).arg(m_audioFrameCount));

    return true;
}

void FFmpegEncoder::cleanup()
{
    LOG_INFO("Cleaning up FFmpegEncoder...");

    m_running = false;
    m_initialized = false;

    // Cleanup video encoder
    if (m_videoFrame) {
        av_frame_free(&m_videoFrame);
    }
    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
    }
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    // Cleanup audio encoder
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    if (m_resampledAudioFrame) {
        av_frame_free(&m_resampledAudioFrame);
    }
    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
    }
    if (m_swrContext) {
        swr_free(&m_swrContext);
    }

    // Cleanup format context
    if (m_formatContext) {
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_formatContext->pb);
        }
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }

    // Clear queues
    {
        QMutexLocker locker(&m_videoQueueMutex);
        m_videoQueue.clear();
    }
    {
        QMutexLocker locker(&m_audioQueueMutex);
        m_audioQueue.clear();
    }

    LOG_INFO("FFmpegEncoder cleanup completed");
}

int FFmpegEncoder::getPendingVideoFrameCount() const
{
    QMutexLocker locker(&m_videoQueueMutex);
    return m_videoQueue.size();
}

int FFmpegEncoder::getPendingAudioFrameCount() const
{
    QMutexLocker locker(&m_audioQueueMutex);
    return m_audioQueue.size();
}

void FFmpegEncoder::freeAVFrame(AVFrame* frame)
{
    if (frame) {
        av_frame_free(&frame);
    }
}

void FFmpegEncoder::freeAVPacket(AVPacket* packet)
{
    if (packet) {
        av_packet_free(&packet);
    }
}
