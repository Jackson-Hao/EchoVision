//
// Created by jackson-hao on 25-3-5.
//

#include "DualLensCamera.h"

CameraConfig cam_config;

DualLensCamera::DualLensCamera(const int device, const int width, const int height,const int fps) :
    cap(device) {
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera!" << std::endl;
        return;
    }
    cam_config.camera_id = device;
    cam_config.width = width;
    cam_config.height = height;
    cam_config.fps = fps;

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FPS, fps);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    std::cout << "Camera settings: " << std::endl;
    std::cout << "Width*Height: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "*" << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
    std::cout << "FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
}

DualLensCamera::~DualLensCamera() {
    cleanup();
}

bool DualLensCamera::isTrueCamera(int width, int height) const {
    if (cap.get(cv::CAP_PROP_FRAME_WIDTH) == width && cap.get(cv::CAP_PROP_FRAME_HEIGHT) == height) {
        return true;
    }
    return false;
}

void DualLensCamera::startRecording(const std::string& folder, int& counter) {
    videoFolder = folder;
    setupVideoWriters(folder, counter);
    recording = true;
}

void DualLensCamera::stopRecording() {
    recording = false;
    writer_left.release();
    writer_right.release();
    writer_merge.release();
}

void DualLensCamera::takeSnapshot(const std::string& folder, int& counter) {
    cv::Mat frame;
    if (readFrame(frame)) {
        cv::Mat left_frame = frame(cv::Rect(0, 0, cam_config.width / 2, cam_config.height));
        cv::Mat right_frame = frame(cv::Rect(cam_config.width / 2, 0, cam_config.width / 2, cam_config.height));

        std::string path = folder + "left_" + std::to_string(counter) + ".jpg";
        cv::imwrite(path, left_frame);
        std::cout << "Snapshot saved into: " << path << std::endl;

        path = folder + "right_" + std::to_string(counter) + ".jpg";
        cv::imwrite(path, right_frame);
        std::cout << "Snapshot saved into: " << path << std::endl;

        cv::Mat merge_frame;
        cv::hconcat(left_frame, right_frame, merge_frame);
        path = folder + "merge_" + std::to_string(counter) + ".jpg";
        cv::imwrite(path, merge_frame);
        std::cout << "Merged snapshot saved into: " << path << std::endl;

        counter++;
    }
}

bool DualLensCamera::readFrame(cv::Mat& frame) {
    if (!cap.read(frame)) {
        std::cerr << "Failed to read frame from camera!" << std::endl;
        return false;
    }
    return true;
}

void DualLensCamera::setupVideoWriters(const std::string& folder, int& counter) {
    // 使用正确的尺寸：宽度为cam_config.width / 2，高度保持cam_config.height不变
    // writer_left.open(folder + "output_left_" + std::to_string(counter) + ".avi",
    //                 cv::VideoWriter::fourcc('M','J','P','G'),
    //                 cam_config.fps,
    //                 cv::Size(cam_config.width / 2, cam_config.height), true); // 宽度除以2，高度不变

    // writer_right.open(folder + "output_right_" + std::to_string(counter) + ".avi",
    //                  cv::VideoWriter::fourcc('M','J','P','G'),
    //                  cam_config.fps,
    //                  cv::Size(cam_config.width / 2, cam_config.height), true); // 宽度除以2，高度不变

    // 对于合并帧，宽度应为cam_config.width，高度保持cam_config.height不变
    writer_merge.open(folder + "output_merge_" + std::to_string(counter) + ".avi",
                      cv::VideoWriter::fourcc('M','J','P','G'),
                      cam_config.fps,
                      cv::Size(cam_config.width /2 , cam_config.height /2), true); // 尺寸与原始帧相同
}

void DualLensCamera::cleanup() {
    if (recording) {
        stopRecording();
    }
}

void DualLensCamera::makeShotFolder(const std::string& folder) {
    struct stat st = {0};
    if (stat(folder.c_str(), &st) == -1) {
        mkdir(folder.c_str(), 0777);
    }
}