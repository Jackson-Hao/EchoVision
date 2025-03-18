#ifndef LIVESTREAM_H
#define LIVESTREAM_H

#include <string>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace LIVE {
    class Streamer {
    public:
        // 构造函数和析构函数
        Streamer(const std::string& rtsp_url, int width, int height, int fps);
        ~Streamer();

        // 初始化推流器
        bool init();

        // 推送帧数据
        void pushFrame(const cv::Mat& frame);

    private:
        // 私有成员变量
        std::string rtsp_url;    // RTSP 地址
        int width, height, fps;  // 视频参数

        SwsContext* sws_context;         // 像素格式转换上下文
        AVCodecContext* codec_context;   // 编码器上下文
        AVFrame* av_frame;               // 视频帧
        AVFormatContext* output_context; // 输出上下文
        AVStream* video_stream;          // 视频流

        // 释放资源
        void releaseResources();
    };
}

#endif // LIVESTREAM_H
