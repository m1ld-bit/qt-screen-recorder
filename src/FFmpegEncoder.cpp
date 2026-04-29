#include "FFmpegEncoder.h"
#include "Logger.h"

FFmpegEncoder::FFmpegEncoder(QObject* parent)
    : QObject(parent)
    , m_formatCtx(nullptr)
    , m_videoCodecCtx(nullptr)
    , m_audioCodecCtx(nullptr)
    , m_videoStream(nullptr)
    , m_audioStream(nullptr)
    , m_swsCtx(nullptr)
    , m_swrCtx(nullptr)
    , m_videoFrame(nullptr)
    , m_audioFrame(nullptr)
    , m_packet(nullptr)
    , m_initialized(false)
    , m_videoStarted(false)
    , m_audioStarted(false)
    , m_videoPts(0)
    , m_audioPts(0)
{
}

FFmpegEncoder::~FFmpegEncoder()
{
    cleanup();
}

void FFmpegEncoder::cleanup()
{
    QMutexLocker locker(&m_mutex);

    if (m_videoFrame) {
        av_frame_free(&m_videoFrame);
    }
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
    }
    if (m_audioCodecCtx) {
        avcodec_free_context(&m_audioCodecCtx);
    }
    if (m_formatCtx) {
        if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_formatCtx->pb);
        }
        avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
    }

    m_initialized = false;
    m_videoStarted = false;
    m_audioStarted = false;
}

bool FFmpegEncoder::init(const EncoderConfig& config)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        cleanup();
    }

    m_config = config;
    m_videoPts = 0;
    m_audioPts = 0;

    if (!openOutputFile()) {
        emit errorOccurred("Failed to open output file");
        return false;
    }

    if (!initVideoStream()) {
        emit errorOccurred("Failed to initialize video stream");
        cleanup();
        return false;
    }

    if (config.recordAudio && !initAudioStream()) {
        emit errorOccurred("Failed to initialize audio stream");
        cleanup();
        return false;
    }

    if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatCtx->pb, m_config.outputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            emit errorOccurred("Failed to open output file for writing");
            cleanup();
            return false;
        }
    }

    if (avformat_write_header(m_formatCtx, nullptr) < 0) {
        emit errorOccurred("Failed to write header");
        cleanup();
        return false;
    }

    m_packet = av_packet_alloc();
    m_initialized = true;

    LOG_INFO("FFmpeg encoder initialized successfully");
    return true;
}

bool FFmpegEncoder::openOutputFile()
{
    avformat_alloc_output_context2(&m_formatCtx, nullptr, nullptr, m_config.outputPath.toUtf8().constData());
    if (!m_formatCtx) {
        avformat_alloc_output_context2(&m_formatCtx, nullptr, "mp4", m_config.outputPath.toUtf8().constData());
    }
    return m_formatCtx != nullptr;
}

bool FFmpegEncoder::initVideoStream()
{
    const AVCodec* videoCodec = avcodec_find_encoder_by_name("libx264");
    if (!videoCodec) {
        videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!videoCodec) {
        LOG_ERROR("H.264 codec not found");
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_videoStream) {
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (!m_videoCodecCtx) {
        return false;
    }

    m_videoCodecCtx->codec_id = videoCodec->id;
    m_videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_videoCodecCtx->width = m_config.width;
    m_videoCodecCtx->height = m_config.height;
    m_videoCodecCtx->time_base = AVRational{1, m_config.frameRate};
    m_videoCodecCtx->framerate = AVRational{m_config.frameRate, 1};
    m_videoCodecCtx->gop_size = 30;
    m_videoCodecCtx->max_b_frames = 2;
    m_videoCodecCtx->bit_rate = m_config.bitrate;

    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* param = nullptr;
    av_dict_set(&param, "preset", "ultrafast", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);

    if (avcodec_open2(m_videoCodecCtx, videoCodec, &param) < 0) {
        av_dict_free(&param);
        return false;
    }
    av_dict_free(&param);

    avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecCtx);
    m_videoStream->time_base = m_videoCodecCtx->time_base;

    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = m_videoCodecCtx->pix_fmt;
    m_videoFrame->width = m_videoCodecCtx->width;
    m_videoFrame->height = m_videoCodecCtx->height;
    av_frame_get_buffer(m_videoFrame, 32);

    return true;
}

bool FFmpegEncoder::initAudioStream()
{
    const AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec) {
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_audioStream) {
        return false;
    }

    m_audioCodecCtx = avcodec_alloc_context3(audioCodec);
    if (!m_audioCodecCtx) {
        return false;
    }

    m_audioCodecCtx->codec_id = audioCodec->id;
    m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    m_audioCodecCtx->sample_rate = m_config.audioSampleRate;
    m_audioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    m_audioCodecCtx->channels = 2;
    m_audioCodecCtx->bit_rate = 128000;
    m_audioCodecCtx->time_base = AVRational{1, m_config.audioSampleRate};

    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(m_audioCodecCtx, audioCodec, nullptr) < 0) {
        return false;
    }

    avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecCtx);
    m_audioStream->time_base = m_audioCodecCtx->time_base;

    m_audioFrame = av_frame_alloc();
    m_audioFrame->format = m_audioCodecCtx->sample_fmt;
    m_audioFrame->channel_layout = m_audioCodecCtx->channel_layout;
    m_audioFrame->sample_rate = m_audioCodecCtx->sample_rate;
    m_audioFrame->nb_samples = m_audioCodecCtx->frame_size;
    av_frame_get_buffer(m_audioFrame, 0);

    return true;
}

bool FFmpegEncoder::encodeVideoFrame(const QImage& image, qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || !m_videoCodecCtx) {
        return false;
    }

    QImage scaledImage = image.scaled(m_config.width, m_config.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage rgbImage = scaledImage.convertToFormat(QImage::Format_RGB32);

    m_swsCtx = sws_getCachedContext(m_swsCtx,
        m_config.width, m_config.height, AV_PIX_FMT_BGRA,
        m_config.width, m_config.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!m_swsCtx) {
        return false;
    }

    uint8_t* srcData[4] = {const_cast<uint8_t*>(rgbImage.bits()), nullptr, nullptr, nullptr};
    int srcStride[4] = {rgbImage.bytesPerLine(), 0, 0, 0};

    sws_scale(m_swsCtx, srcData, srcStride, 0, m_config.height,
              m_videoFrame->data, m_videoFrame->linesize);

    m_videoFrame->pts = m_videoPts++;

    if (avcodec_send_frame(m_videoCodecCtx, m_videoFrame) < 0) {
        return false;
    }

    while (true) {
        int ret = avcodec_receive_packet(m_videoCodecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            return false;
        }

        av_packet_rescale_ts(m_packet, m_videoCodecCtx->time_base, m_videoStream->time_base);
        m_packet->stream_index = m_videoStream->index;
        av_interleaved_write_frame(m_formatCtx, m_packet);
        av_packet_unref(m_packet);
    }

    return true;
}

bool FFmpegEncoder::encodeAudioFrame(const QByteArray& audioData, qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || !m_audioCodecCtx || !m_config.recordAudio) {
        return false;
    }
    return true;
}

bool FFmpegEncoder::finish()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) {
        return false;
    }

    if (m_videoCodecCtx) {
        avcodec_send_frame(m_videoCodecCtx, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_videoCodecCtx, m_packet);
            if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }
            av_packet_rescale_ts(m_packet, m_videoCodecCtx->time_base, m_videoStream->time_base);
            m_packet->stream_index = m_videoStream->index;
            av_interleaved_write_frame(m_formatCtx, m_packet);
            av_packet_unref(m_packet);
        }
    }

    if (m_audioCodecCtx) {
        avcodec_send_frame(m_audioCodecCtx, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_audioCodecCtx, m_packet);
            if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }
            av_packet_rescale_ts(m_packet, m_audioCodecCtx->time_base, m_audioStream->time_base);
            m_packet->stream_index = m_audioStream->index;
            av_interleaved_write_frame(m_formatCtx, m_packet);
            av_packet_unref(m_packet);
        }
    }

    writeTrailer();
    cleanup();

    LOG_INFO("Recording finished successfully");
    return true;
}

void FFmpegEncoder::writeTrailer()
{
    if (m_formatCtx) {
        av_write_trailer(m_formatCtx);
    }
}

