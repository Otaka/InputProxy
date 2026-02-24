#include "UartManagerPico.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include <string.h>

// Global array to track UartManager instances
static UartManagerPico* uartManagers[2] = {nullptr, nullptr};

// Static interrupt handlers
void UartManagerPico::on_uart_interrupt_0() {
    if (uartManagers[0]) {
        uartManagers[0]->onInterrupt();
    }
}

void UartManagerPico::on_uart_interrupt_1() {
    if (uartManagers[1]) {
        uartManagers[1]->onInterrupt();
    }
}

UartManagerPico::UartManagerPico(UartPort port) 
    : port(port), 
      bufferIndex(0),
      messageCallback(nullptr) {

    // Initialize mutex
    mutex_init(&sendMutex);

    // Get the UART instance
    uart_inst_t* uartId = (port == UartPort::UART_0) ? uart0 : uart1;
    int portIndex = (port == UartPort::UART_0) ? 0 : 1;

    // Store this instance in the global array
    uartManagers[portIndex] = this;

    // Initialize UART
    uart_init(uartId, 115200);

    // Set GPIO pins based on port
    if (port == UartPort::UART_0) {
        gpio_set_function(0, GPIO_FUNC_UART);  // TX
        gpio_set_function(1, GPIO_FUNC_UART);  // RX
    } else {
        gpio_set_function(4, GPIO_FUNC_UART);  // TX
        gpio_set_function(5, GPIO_FUNC_UART);  // RX
    }

    // Set up interrupt handler
    if (port == UartPort::UART_0) {
        irq_set_exclusive_handler(UART0_IRQ, on_uart_interrupt_0);
        irq_set_enabled(UART0_IRQ, true);
    } else {
        irq_set_exclusive_handler(UART1_IRQ, on_uart_interrupt_1);
        irq_set_enabled(UART1_IRQ, true);
    }

    // Enable UART RX interrupt
    uart_set_irq_enables(uartId, true, false);
}

UartManagerPico::~UartManagerPico() {
    int portIndex = (port == UartPort::UART_0) ? 0 : 1;
    uartManagers[portIndex] = nullptr;

    // Disable interrupts
    uart_inst_t* uartId = (port == UartPort::UART_0) ? uart0 : uart1;
    uart_set_irq_enables(uartId, false, false);

    if (port == UartPort::UART_0) {
        irq_set_enabled(UART0_IRQ, false);
    } else {
        irq_set_enabled(UART1_IRQ, false);
    }
}

void UartManagerPico::onMessage(MessageCallback callback) {
    messageCallback = callback;
}

void UartManagerPico::sendData(const char* data, size_t length) {
    if (length == 0 || data == nullptr) {
        return;
    }

    // Lock mutex to protect UART sending
    mutex_enter_blocking(&sendMutex);

    uart_inst_t* uartId = (port == UartPort::UART_0) ? uart0 : uart1;
    uart_write_blocking(uartId, (const uint8_t*)data, length);

    mutex_exit(&sendMutex);
}

void UartManagerPico::onInterrupt() {
    uart_inst_t* uartId = (port == UartPort::UART_0) ? uart0 : uart1;

    // Read available bytes into buffer
    bufferIndex = 0;
    while (uart_is_readable(uartId) && bufferIndex < sizeof(buffer)) {
        buffer[bufferIndex++] = uart_getc(uartId);
    }
    
    // Call callback with raw data if we received anything
    if (messageCallback && bufferIndex > 0) {
        messageCallback(buffer, bufferIndex);
    }
}