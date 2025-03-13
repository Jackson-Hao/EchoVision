#ifndef GNSS_H
#define GNSS_H

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iomanip>
#include <variant>

#include "HAL_UART.h"



namespace GNSS {
    // 解析 GNGGA 句子
    typedef struct {
        std::string time;       // UTC 时间 (hhmmss.sss)
        double latitude;        // 纬度
        char lat_direction;     // 纬度方向 (N/S)
        double longitude;       // 经度
        char lon_direction;     // 经度方向 (E/W)
        int quality;            // 定位质量
        int satellites;         // 使用的卫星数量
        double hdop;            // 水平精度因子
        double altitude;        // 海拔高度
        double geoid_height;    // 地球椭球高度
    } GNGGA;

    // 解析 GNRMC 句子
    typedef struct {
        std::string time;       // UTC 时间 (hhmmss.sss)
        char status;            // 状态 (A=有效, V=无效)
        double latitude;        // 纬度
        char lat_direction;     // 纬度方向 (N/S)
        double longitude;       // 经度
        char lon_direction;     // 经度方向 (E/W)
        double speed;           // 地面速度 (节)
        double course;          // 航向 (度)
        std::string date;       // 日期 (ddmmyy)
    } GNRMC;
    class Location {
        public:
        Location();
        ~Location();
        void locationService(HAL::UART::Uart& uart);
        bool parseGNGGA(const std::string& sentence, GNGGA& gngga);
        bool parseGNRMC(const std::string& sentence, GNRMC& gnrmc);
        void printInfo(const GNGGA& gngga);
        void printInfo(const GNRMC& gnrmc);
    };
    class MessageQueue {
    private:
        std::queue<std::variant<GNGGA, GNRMC>> queue; // 存储解析后的数据
        std::mutex mtx;
        std::condition_variable condition_v;
        bool stopFlag = false; // 停止标志

    public:
        // 添加消息到队列
        template <typename T>
        void push(const T& message) {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(message);
            condition_v.notify_one(); // 通知等待的线程
        }

        // 从队列中获取消息
        std::variant<GNGGA, GNRMC> pop() {
            std::unique_lock<std::mutex> lock(mtx);
            condition_v.wait(lock, [this] { return !queue.empty() || stopFlag; }); // 等待直到队列非空或停止
            if (!queue.empty()) {
                auto message = queue.front();
                queue.pop();
                return message;
            }
            return {};
        }

        // 设置停止标志
        void stop() {
            {
                std::lock_guard<std::mutex> lock(mtx);
                stopFlag = true;
            }
            condition_v.notify_all(); // 通知所有等待的线程
        }
    };
}


#endif //GNSS_H
