#include "HAL_UART.h"
#include <fcntl.h>
#include <termios.h>
#include <condition_variable>
#include <unistd.h>
#include <cstring> // memset

using namespace HAL::UART;

Uart::Uart() : fd_(-1) {}

Uart::~Uart() {
    if (fd_ != -1) {
        close(fd_);
    }
}

void Uart::setOptions(int fd, const Config& config) {
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        // 错误处理逻辑
    }

    cfsetospeed(&tty, config.baudRate);
    cfsetispeed(&tty, config.baudRate);

    tty.c_cflag &= ~PARENB; // 清除奇偶校验位
    tty.c_cflag &= ~CSTOPB; // 设置停止位为1
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 设置数据位为8
    tty.c_cflag &= ~CRTSCTS; // 禁用硬件流控制
    tty.c_cflag |= CREAD | CLOCAL; // 打开接收器，忽略调制解调器控制线

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // 禁用回显
    tty.c_lflag &= ~ECHOE; // 禁用错误回显
    tty.c_lflag &= ~ECHONL; // 禁用换行符回显
    tty.c_lflag &= ~ISIG; // 禁用解释输入中的信号

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 禁用软件流控制
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // 输入选项

    tty.c_oflag &= ~OPOST; // 原始输出模式

    tty.c_cc[VTIME] = 10;    // 设置等待时间
    tty.c_cc[VMIN] = 0;

    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        // 错误处理逻辑
    }
}

HAL::HardwareStatus Uart::uartInit(const Config& config) {
    if (fd_ != -1) {
        close(fd_);
    }
    fd_ = open(config.device.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        return HAL::ERROR;
    }
    setOptions(fd_, config);

    return HAL::OK;
}

HAL::HardwareStatus Uart::uartWrite(const uint8_t* data, size_t size) const {
    ssize_t written = write(fd_, data, size);
    if (written == static_cast<ssize_t>(size)) {
        return HAL::OK;
    } else {
        return HAL::ERROR;
    }
}

std::string Uart::uartReadLine() {
    char buffer[256];
    ssize_t bytesRead;

    while (true) {
        bytesRead = read(fd_, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // 确保字符串以 null 结尾
            this->buffer += buffer;   // 将读取到的数据添加到内部缓冲区

            // 查找换行符并提取完整行
            size_t pos = this->buffer.find('\n');
            if (pos != std::string::npos) {
                std::string line = this->buffer.substr(0, pos);
                this->buffer = this->buffer.substr(pos + 1); // 去掉已提取的行
                return line;
            }
        } else if (bytesRead == -1 && errno != EAGAIN) {
            std::cerr << "Read error: " << strerror(errno) << std::endl;
            return ""; // 返回空字符串表示读取失败
        }
    }
}

HAL::HardwareStatus Uart::uartGetStatus() const {
    // 这里可以添加更详细的错误检测逻辑
    return fd_ != -1 ? HAL::OK : HAL::ERROR;
}