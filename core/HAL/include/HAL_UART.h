#ifndef HAL_UART_H
#define HAL_UART_H

#include "HAL.h"
#include <iostream>
#include <cstdint>


namespace HAL::UART{
    struct Config {
        std::string device;  // 设备路径，例如 "/dev/ttyS0"
        unsigned int baudRate;
        char parity;         // 'N' - None, 'E' - Even, 'O' - Odd
        unsigned int dataBits;
        unsigned int stopBits;
    };
    class Uart {
    public:
        Uart(); // 构造函数
        ~Uart(); // 析构函数

        HAL::HardwareStatus uartInit(const Config& config);
        HAL::HardwareStatus uartWrite(const uint8_t* data, size_t size) const;
        std::string uartReadLine();
        [[nodiscard]] HAL::HardwareStatus uartGetStatus() const;
    private:
        int fd_; // 文件描述符
        std::string buffer; // 缓冲区用于存储未完成的行数据
        static void setOptions(int fd, const Config& config);
        // 禁止拷贝构造和赋值操作
    public:
        Uart(const Uart&) = delete;
        Uart& operator=(const Uart&) = delete;
    };
}

#endif //HAL_UART_H
