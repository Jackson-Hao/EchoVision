#include "GNSS.h"
#include "HAL_UART.h"
#include <iostream>


GNSS::Location::Location() {
    // 初始化代码
    std::cout << "GNSS Location initialized." << std::endl;
}

GNSS::Location::~Location() = default;

void GNSS::Location::locationService(HAL::UART::Uart &uart) {
    MessageQueue messageQueue;
    std::thread readerThread([&]() {
        while (true) {
            std::string line = uart.uartReadLine();
            if (line.empty()) continue;
            if (line.find("$GNGGA") != std::string::npos) {
                GNGGA gngga;
                if (parseGNGGA(line, gngga)) {
                    messageQueue.push(gngga);
                }
            } else if (line.find("$GNRMC") != std::string::npos) {
                GNRMC gnrmc;
                if (parseGNRMC(line, gnrmc)) {
                    messageQueue.push(gnrmc);
                }
            }
        }
    });
    std::thread writerThread([&]() {
        while (true) {
            auto message = messageQueue.pop();
            if (std::holds_alternative<GNGGA>(message)) {
                printInfo(std::get<GNGGA>(message));
            } else if (std::holds_alternative<GNRMC>(message)) {
                printInfo(std::get<GNRMC>(message));
            } else {
                break; // 如果接收到空值，退出循环
            }
        }
    });

    readerThread.join();
    writerThread.join();
}

bool GNSS::Location::parseGNGGA(const std::string& sentence, GNGGA& gngga) {
    std::istringstream iss(sentence);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(iss, token, ',')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 15 || tokens[0] != "$GNGGA") return false;

    gngga.time = tokens[1];
    gngga.latitude = tokens[2].empty() ? 0.0 : std::stod(tokens[2]) / 100.0;
    gngga.lat_direction = tokens[3][0];
    gngga.longitude = tokens[4].empty() ? 0.0 : std::stod(tokens[4]) / 100.0;
    gngga.lon_direction = tokens[5][0];
    gngga.quality = std::stoi(tokens[6]);
    gngga.satellites = std::stoi(tokens[7]);
    gngga.hdop = tokens[8].empty() ? 0.0 : std::stod(tokens[8]);
    gngga.altitude = tokens[9].empty() ? 0.0 : std::stod(tokens[9]);
    gngga.geoid_height = tokens[11].empty() ? 0.0 : std::stod(tokens[11]);

    return true;
}

bool GNSS::Location::parseGNRMC(const std::string& sentence, GNRMC& gnrmc) {
    std::istringstream iss(sentence);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(iss, token, ',')) {
        tokens.push_back(token);
    }


    if (tokens.size() < 13 || tokens[0] != "$GNRMC") return false;

    gnrmc.time = tokens[1];
    gnrmc.status = tokens[2][0];
    gnrmc.latitude = tokens[3].empty() ? 0.0 : std::stod(tokens[3]) / 100.0;
    gnrmc.lat_direction = tokens[4][0];
    gnrmc.longitude = tokens[5].empty() ? 0.0 : std::stod(tokens[5]) / 100.0;
    gnrmc.lon_direction = tokens[6][0];
    gnrmc.speed = tokens[7].empty() ? 0.0 : std::stod(tokens[7]);
    gnrmc.course = tokens[8].empty() ? 0.0 : std::stod(tokens[8]);
    gnrmc.date = tokens[9];

    return true;
}


void GNSS::Location::printInfo(const GNGGA& gngga) {
    std::cout << "GNGGA 定位信息:" << std::endl;
    std::cout << "UTC时间: " << gngga.time << std::endl;
    std::cout << "纬度: " << gngga.latitude << " " << gngga.lat_direction << std::endl;
    std::cout << "经度: " << gngga.longitude << " " << gngga.lon_direction << std::endl;
    std::cout << "定位质量: " << gngga.quality << std::endl;
    std::cout << "卫星数量: " << gngga.satellites << std::endl;
    std::cout << "海拔高度: " << gngga.altitude << " 米" << std::endl;
    std::cout << "地球椭球高度: " << gngga.geoid_height << " 米" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

void GNSS::Location::printInfo(const GNRMC& gnrmc) {
    std::cout << "GNRMC 定位信息:" << std::endl;
    std::cout << "时间: " << gnrmc.time << std::endl;
    std::cout << "日期: " << gnrmc.date << std::endl;
    std::cout << "状态: " << (gnrmc.status == 'A' ? "有效" : "无效") << std::endl;
    std::cout << "纬度: " << gnrmc.latitude << " " << gnrmc.lat_direction << std::endl;
    std::cout << "经度: " << gnrmc.longitude << " " << gnrmc.lon_direction << std::endl;
    std::cout << "速度: " << gnrmc.speed << " 节" << std::endl;
    std::cout << "航向: " << gnrmc.course << " 度" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}