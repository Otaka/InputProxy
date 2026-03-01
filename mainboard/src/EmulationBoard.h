#pragma once

#include <string>
#include "UartManager.h"
#include "rpcinterface.h"

struct EmulationBoard {
    int id;
    std::string serialString;
    Main2Pico main2PicoRpcClient;
    UART_CHANNEL uartChannel;
    bool active;
};
