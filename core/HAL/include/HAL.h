//
// Created by jackson-hao on 25-3-5.
//

#ifndef HAL_H
#define HAL_H

namespace HAL {
    enum HardwareStatus {
        OK,
        ERROR,
        TIMEOUT,
        BUSY
    };
    namespace GPIO{}
    namespace I2C{}
    namespace SPI{}
    namespace UART{}
}

#include "HAL_GPIO.h"
#include "HAL_UART.h"

#endif //HAL_H
