#include "LiveStream.h"
#include <iostream>
#include <thread>

namespace LIVE {

Streamer::Streamer(const std::string& rtsp_url, int width, int height, int fps)
    : rtsp_url(rtsp_url), width(width), height(height), fps(fps),
      sws_context(nullptr), codec_context(nullptr), av_frame(nullptr),
      output_context(nullptr) {}

Streamer::~Streamer() {
    releaseResources();
}

bool Streamer::init() {
    // 初始化 FFmpeg
    avformat_network_init();

    // 创建输出上下文
    if (avformat_alloc_output_context2(&output_context, nullptr, "rtsp", rtsp_url.c_str()) < 0) {
        std::cerr << "无法创建输出上下文！" << std::endl;
        return false;
    }

    // 创建视频流
    video_stream = avformat_new_stream(output_context, nullptr);
    if (!video_stream) {
        std::cerr << "无法创建视频流！" << std::endl;
        return false;
    }

    // 设置编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "无法找到编码器！" << std::endl;
        return false;
    }

    codec_context = avcodec_alloc_context3(codec);
    codec_context->width = width;
    codec_context->height = height;
    codec_context->time_base = {1, fps};
    codec_context->framerate = {fps, 1};
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->bit_rate = 500 * 1000;
    codec_context->gop_size = 25;
    codec_context->max_b_frames = 1;
    codec_context->thread_count = std::thread::hardware_concurrency();
    codec_context->thread_type = FF_THREAD_SLICE;

    if (output_context->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_opt_set(codec_context->priv_data, "preset", "fast", 0);
    av_opt_set(codec_context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_context->priv_data, "profile", "baseline", 0);

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        std::cerr << "无法打开编码器！" << std::endl;
        return false;
    }

    avcodec_parameters_from_context(video_stream->codecpar, codec_context);

    if (!(output_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_context->pb, rtsp_url.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "无法打开 RTSP 地址！" << std::endl;
            return false;
        }
    }

    if (avformat_write_header(output_context, nullptr) < 0) {
        std::cerr << "无法写入头信息！" << std::endl;
        return false;
    }

    // 初始化像素格式转换上下文
    sws_context = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    // 初始化帧
    av_frame = av_frame_alloc();
    av_frame->format = AV_PIX_FMT_YUV420P;
    av_frame->width = width;
    av_frame->height = height;
    av_image_alloc(av_frame->data, av_frame->linesize, width, height, AV_PIX_FMT_YUV420P, 32);

    return true;
}

void Streamer::pushFrame(const cv::Mat& frame) {
    if (frame.empty()) {
        std::cout << "空帧，无法推送！" << std::endl;
    } else if (frame.channels() == 1) {
        std::cout << "单通道图像，无法推送！" << std::endl;
    }
    else if (frame.type() != CV_8UC3) {
        std::cout << "图像格式不支持，无法推送！" << std::endl;
    }
    else if (frame.size() != cv::Size(width, height)) {
        std::cout << "图像尺寸不匹配，无法推送！" << std::endl;
    }
    else if (av_frame == nullptr) {
        std::cout << "帧未初始化，无法推送！" << std::endl;
    }
    else if (sws_context == nullptr) {
        std::cout << "像素格式转换上下文未初始化，无法推送！" << std::endl;
    }
    else if (codec_context == nullptr) {
        std::cout << "编码器上下文未初始化，无法推送！" << std::endl;
    }
    else if (output_context == nullptr) {
        std::cout << "输出上下文未初始化，无法推送！" << std::endl;
    }

    uint8_t* src_data[1] = { frame.data };
    int src_linesize[1] = { static_cast<int>(frame.step) };

    // 转换像素格式
    sws_scale(sws_context, src_data, src_linesize, 0, height, av_frame->data, av_frame->linesize);

    // 编码并推流
    AVPacket pkt = {0};
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;

    av_frame->pts++;
    if (avcodec_send_frame(codec_context, av_frame) == 0) {
        while (avcodec_receive_packet(codec_context, &pkt) == 0) {
            av_interleaved_write_frame(output_context, &pkt);
            av_packet_unref(&pkt);
        }
    }
}

void Streamer::releaseResources() {
    if (av_frame) av_frame_free(&av_frame);
    if (sws_context) sws_freeContext(sws_context);
    if (codec_context) avcodec_free_context(&codec_context);
    if (output_context) {
        av_write_trailer(output_context);
        if (!(output_context->oformat->flags & AVFMT_NOFILE)) avio_closep(&output_context->pb);
        avformat_free_context(output_context);
    }
    avformat_network_deinit();
}

} // namespace LIVE
