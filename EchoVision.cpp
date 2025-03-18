#include "EchoVision.h"
#include "DualLensCamera.h"
#include "HAL_UART.h"

#include <condition_variable>
#include "GNSS.h"
#include "ONNX.h"
#include "LiveStream.h"
#include <thread>
#include <queue>
#include <mutex>

std::queue<cv::Mat> frameQueue;
std::mutex queueMutex;
std::condition_variable queueCV;
bool stopThreads = false;

#define VISUAL

#define CONFIDENCE_THRESHOLD 0.35
#define IOU_THRESHOLD 0.45

std::string COCO_YAML_PATH = "./coco8.yaml";
std::string YOLO_MODEL_PATH = "./yolo11n_dynamic.onnx";

HAL::UART::Config config;
HAL::UART::Uart uart;

namespace HW {
    static void HardwareInit() {
        config.device = "/dev/ttyUSB0";
        config.baudRate = 115200;
        config.parity = 'N';
        config.dataBits = 8;
        config.stopBits = 1;

        uart.uartInit(config);
        if (const HAL::HardwareStatus status = uart.uartGetStatus(); status != HAL::HardwareStatus::OK) {
            std::cerr << "UART initialization failed." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    static void HardwareService() {
        GNSS::Location gnss;
        gnss.locationService(uart);
    }
}

#ifdef __VISUAL
namespace VS {
    ONNX::YOLO yolo(YOLO_MODEL_PATH, COCO_YAML_PATH);
    static void displayVideo(const cv::Mat& frame) {
        // 分割帧为左右两部分
        cv::Mat leftFrame = frame(cv::Rect(0, 0, CAM_WIDTH / 2, CAM_HEIGHT));
        cv::Mat rightFrame = frame(cv::Rect(CAM_WIDTH / 2, 0, CAM_WIDTH / 2, CAM_HEIGHT));

        cv::Mat leftAfterDetect = yolo.yoloDetect(leftFrame);
        // 水平连接左右两部分
        cv::Mat mergeFrame;
        hconcat(leftAfterDetect, rightFrame, mergeFrame);

        cv::resize(mergeFrame, mergeFrame, cv::Size(), 0.67, 0.67);
        // 显示结果
        imshow("Dual Lens Camera", mergeFrame);
        cv::waitKey(1); // 等待1毫秒以更新窗口
    }
}
#endif

namespace Camera {
    static DualLensCamera CameraServiceInit() {
        DualLensCamera cam(CAM_ID, CAM_WIDTH, CAM_HEIGHT, CAM_FPS);

        if (!cam.isTrueCamera(CAM_WIDTH, CAM_HEIGHT)) {
            std::cerr << "Camera initialization failed: Invalid camera settings." << std::endl;
            exit(EXIT_FAILURE);
        }
        return cam;
    }

    static int cameraService(const cv::Mat& frame) {
        DualLensCamera::makeShotFolder(PICTURE_DIR);
        DualLensCamera::makeShotFolder(VIDEO_DIR);
        VS::displayVideo(frame);
        return EXIT_SUCCESS;
    }
}

namespace Stream {
    static cv::Mat CutFrame(const cv::Mat &frame) {
        cv::Mat right_frame = frame(cv::Rect(CAM_WIDTH / 2, 0, CAM_WIDTH / 2, CAM_HEIGHT));
        cv::Mat resized_right;
        cv::resize(right_frame, resized_right, cv::Size(1280, 720));
        return resized_right;    // 返回大小为1280x720的右侧图像
    }

    static LIVE::Streamer StreamServiceInit(const std::string& rtsp_url, int width, int height, int fps) {
        LIVE::Streamer streamer(rtsp_url, width, height, fps);
        if (!streamer.init()) {
            std::cerr << "Failed to initialize streamer." << std::endl;
            exit(EXIT_FAILURE);
        }
        return streamer;
    }

    static int StreamService(LIVE::Streamer& streamer , const cv::Mat &frame) {
        cv::Mat frame1 = CutFrame(frame);
        streamer.pushFrame(frame1);
        return EXIT_SUCCESS;
    }
}


static void captureFrames(DualLensCamera &cam) {
    while (!stopThreads) {
        cv::Mat frame;
        if (!cam.readFrame(frame)) {
            std::cerr << "Failed to read frame from camera." << std::endl;
            break;
        }

        // 将帧加入共享队列
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            frameQueue.push(frame);
        }
        queueCV.notify_all();
    }
}

static void displayFrames() {
    while (!stopThreads) {
        cv::Mat frame;

        // 从队列中取出帧用于显示
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [] { return !frameQueue.empty() || stopThreads; });

            if (stopThreads && frameQueue.empty()) break;

            frame = frameQueue.front();
            frameQueue.pop();
        }

        // 显示帧
        Camera::cameraService(frame);
    }
}

static void streamFrames(LIVE::Streamer& streamer) {
    while (!stopThreads) {
        cv::Mat frame;

        // 从队列中取出帧用于传输
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [] { return !frameQueue.empty() || stopThreads; });

            if (stopThreads && frameQueue.empty()) break;

            frame = frameQueue.front();
            frameQueue.pop();
        }

        // 传输视频帧
        Stream::StreamService(streamer, frame);
    }
}


static void IoTMainTaskEntry() {
    auto cam = Camera::CameraServiceInit();
    auto streamer = Stream::StreamServiceInit("rtsp://127.0.0.1:8554/camera_test", 1280, 720, 30);
    // 创建捕获线程、显示线程和传输线程
    std::thread captureThread(captureFrames, std::ref(cam));
    std::thread displayThread(displayFrames);
    std::thread streamThread(streamFrames, std::ref(streamer));

    captureThread.join();
    displayThread.join();
    streamThread.join();
}

APP_SERVICE_INIT(IoTMainTaskEntry);