#pragma once
#include <cstdint>
#include "../shared/corocgo.h"

using namespace corocgo;

class UartManagerPico {
public:
    UartManagerPico();
    ~UartManagerPico();

    size_t read(char* buffer, size_t bufferSize);
    void sendData(const char* data, size_t length);

private:
    static void on_uart_interrupt_0();

    void onInterrupt();

    Channel<bool>* channel;
};
