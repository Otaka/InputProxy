#pragma once
#include "pico/time.h"
#include <functional>
#include <cstddef>

class UsbCdcManager {
public:
    using MessageCallback = std::function<void(const char* buffer, size_t length)>;

    static UsbCdcManager& get();
    
    void initialize();
    void stop();
    void onMessage(MessageCallback callback);
    void sendMessage(const char* buffer, size_t length);
    void sendMessage(const char* str); // Convenience overload for null-terminated strings
    void processUsbData(); // Make public so main loop can call it

    // Delete copy constructor and assignment operator
    UsbCdcManager(const UsbCdcManager&) = delete;
    UsbCdcManager& operator=(const UsbCdcManager&) = delete;

private:
    UsbCdcManager();
    ~UsbCdcManager();

    MessageCallback messageCallback;
    bool initialized;
};