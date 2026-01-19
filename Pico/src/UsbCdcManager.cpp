#include "UsbCdcManager.h"
#include "tusb.h"

#include <cstring>
#include <cstdio>

UsbCdcManager::UsbCdcManager() 
    : messageCallback(nullptr), initialized(false) {
}

UsbCdcManager::~UsbCdcManager() {
    stop();
}

UsbCdcManager& UsbCdcManager::get() {
    static UsbCdcManager instance;
    return instance;
}

void UsbCdcManager::initialize() {
    if (!initialized) {
        // No timer needed - main loop will handle tud_task() and we'll poll in main loop
        initialized = true;
    }
}

void UsbCdcManager::stop() {
    if (initialized) {
        initialized = false;
    }
}

void UsbCdcManager::onMessage(MessageCallback callback) {
    messageCallback = callback;
}

void UsbCdcManager::sendMessage(const char* buffer, size_t length) {
    if (!tud_cdc_connected()) {
        return;
    }
    
    // Write all data, handling buffer full cases
    size_t sent = 0;
    while (sent < length) {
        uint32_t available = tud_cdc_write_available();
        if (available == 0) {
            // Buffer full - flush and wait for USB processing
            tud_cdc_write_flush();
            tud_task();
            continue;
        }
        
        // Write as much as possible
        uint32_t to_write = (length - sent < available) ? (length - sent) : available;
        sent += tud_cdc_write(buffer + sent, to_write);
    }
    
    // Final flush
    tud_cdc_write_flush();
    tud_task();
}

void UsbCdcManager::sendMessage(const char* str) {
    sendMessage(str, strlen(str));
}

void UsbCdcManager::processUsbData() {
    if (tud_cdc_available() && messageCallback) {
        char buf[256];
        int n = tud_cdc_read(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0'; // Null-terminate the buffer
            messageCallback(buf, n);
        }
    }
}