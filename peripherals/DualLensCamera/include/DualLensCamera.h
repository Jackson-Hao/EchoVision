//
// Created by jackson-hao on 25-3-5.
//

#ifndef DUALLENSCAMERA_H
#define DUALLENSCAMERA_H

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <sys/stat.h>

class DualLensCamera {
public:
    DualLensCamera(int device, int width, int height, int fps);
    ~DualLensCamera();

    [[nodiscard]] bool isTrueCamera(int width, int height) const;
    void startRecording(const std::string& folder, int& counter);
    void stopRecording();
    void takeSnapshot(const std::string& folder, int& counter);
    bool readFrame(cv::Mat& frame);
    void setupVideoWriters(const std::string& folder, int& counter);
    void cleanup();
    static void makeShotFolder(const std::string& folder);

    cv::VideoCapture cap;
    cv::VideoWriter writer_left, writer_right, writer_merge;
    std::string videoFolder;
    bool recording = false;
};

typedef struct {
    int camera_id;
    int width;
    int height;
    int fps;
} CameraConfig;


#endif //DUALLENSCAMERA_H
