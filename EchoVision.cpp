#include "EchoVision.h"
#include "DualLensCamera.h"
#include "HAL_UART.h"
#include <mutex>
#include <condition_variable>
#include "GNSS.h"
#include "ONNX.h"
#include "LiveStream.h"

#define VISUAL

#define CONFIDENCE_THRESHOLD 0.35
#define IOU_THRESHOLD 0.45


std::string COCO_YAML_PATH = "./coco8.yaml";
std::string YOLO_MODEL_PATH = "./yolo11n_dynamic.onnx";
bool shouldTakeSnapshot = false;
bool shouldStartRecording = false;
std::mutex mtx;
std::condition_variable con_cv;
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

namespace Console {
    static void consoleService() {
        while (true) {
            char key;
            std::cout << "Press 's' to take snapshot, 'v' to start recording a 10s video, 'q' to quit." << std::endl;
            std::cin >> key;
            if (key == 'q') {
                exit(0);
            }
            else if (key == 's') {
                std::lock_guard<std::mutex> lock(mtx);
                shouldTakeSnapshot = true;
                con_cv.notify_one();
            }
            else if (key == 'v') {
                std::lock_guard<std::mutex> lock(mtx);
                shouldStartRecording = true;
                con_cv.notify_one();
            }
        }
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
    void handleSnapshot(DualLensCamera& cam, int& counter, std::mutex& mtx) {
        std::lock_guard<std::mutex> lock(mtx);
        if (shouldTakeSnapshot) {
            cam.takeSnapshot("./SaveImage/", counter);
            shouldTakeSnapshot = false;
        }
    }

    bool startRecording(DualLensCamera& cam, int& counter, bool& recordingStarted, std::mutex& mtx) {
        std::lock_guard<std::mutex> lock(mtx);
        if (shouldStartRecording && !recordingStarted) {
            cam.startRecording("./SaveVideo/", counter); // 计数器在这里总是从0开始
            recordingStarted = true;
            shouldStartRecording = false;
            std::cout << "Recording started." << std::endl;
            counter++;
            return true;
        }
        return false;
    }

    void processRecording(DualLensCamera& cam, const int totalFrames) {
        int counter = 0;
        // auto start = std::chrono::steady_clock::now();
        while (counter < totalFrames) {
            //auto frameStart = std::chrono::steady_clock::now();

            cv::Mat frame;
            if (!cam.readFrame(frame)) break; // 如果无法读取帧，则退出
            cv::Mat left_frame = frame(cv::Rect(0, 0, CAM_WIDTH / 2, CAM_HEIGHT));
            cv::Mat right_frame = frame(cv::Rect(CAM_WIDTH / 2, 0, CAM_WIDTH / 2, CAM_HEIGHT));

            cv::Mat resized_left, resized_right;

            cv::resize(left_frame, resized_left, cv::Size(CAM_WIDTH / 4, CAM_HEIGHT / 2));
            cv::resize(right_frame, resized_right, cv::Size(CAM_WIDTH / 4, CAM_HEIGHT / 2));
            // cam.writer_left.write(left_frame);
            // cam.writer_right.write(right_frame);
            cv::Mat merge_frame;
            cv::hconcat(resized_left, resized_right, merge_frame);
            cam.writer_merge.write(merge_frame);
            counter++;
        }

        cam.stopRecording();
        std::cout << "Video recording completed after writing " << counter << " frames." << std::endl;
    }

    static int cameraService() {
        DualLensCamera cam(CAM_ID, CAM_WIDTH, CAM_HEIGHT, CAM_FPS);

        if (!cam.isTrueCamera(CAM_WIDTH, CAM_HEIGHT)) {
            std::cerr << "Camera initialization failed: Invalid camera settings." << std::endl;
            return EXIT_FAILURE;
        }

        DualLensCamera::makeShotFolder(PICTURE_DIR);
        DualLensCamera::makeShotFolder(VIDEO_DIR);

        int counter = 0;
        bool recordingStarted = false;
        std::mutex mtx;

#ifndef __STREAM
        LIVE::Streamer streamer("rtsp://127.0.0.1/camera_test", 1280, 720, 30);
        if (!streamer.init()) {
            std::cerr << "Failed to initialize streamer." << std::endl;
            return EXIT_FAILURE;
        }
#endif
        while (true) {
            cv::Mat frame;
            if (!cam.readFrame(frame)) break;
            handleSnapshot(cam, counter, mtx);
#ifdef __VISUAL
            VS::displayVideo(frame);
#endif
#ifndef __STREAM
            // 这里可以添加推流代码
            streamer.pushFrame(frame);
#endif

            if (startRecording(cam, counter, recordingStarted, mtx)) {
                constexpr int totalFrames = 300;
                processRecording(cam, totalFrames);
                recordingStarted = false; // Reset the flag after finishing recording
            }
        }
        return EXIT_SUCCESS;
    }
}

static void IoTMainTaskEntry() {
    // HW::HardwareInit();
    std::thread threadConsole(Console::consoleService);
    std::thread threadCamera(Camera::cameraService);
    // HW::HardwareService();
    threadConsole.join();
    threadCamera.join();
}

APP_SERVICE_INIT(IoTMainTaskEntry);