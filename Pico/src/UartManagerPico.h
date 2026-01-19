#pragma once
#include <functional>
#include <cstdint>
#include "pico/mutex.h"

enum class UartPort {
    UART_0,
    UART_1
};

class UartManagerPico {
public:
    using MessageCallback = std::function<void(const char* data, size_t length)>;

    UartManagerPico(UartPort port);
    ~UartManagerPico();

    void onMessage(MessageCallback callback);
    void sendData(const char* data, size_t length);

private:
    static void on_uart_interrupt_0();
    static void on_uart_interrupt_1();
    
    void onInterrupt();
    
    UartPort port;
    MessageCallback messageCallback;
    char buffer[2048];
    size_t bufferIndex;
    mutex_t sendMutex;
};