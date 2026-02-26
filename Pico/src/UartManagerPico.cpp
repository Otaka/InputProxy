#include "UartManagerPico.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

static UartManagerPico* instance = nullptr;

void UartManagerPico::on_uart_interrupt_0() {
    if (instance) {
        instance->onInterrupt();
    }
}

UartManagerPico::UartManagerPico() {
    channel = makeChannel<bool>(1, 4);
    instance = this;

    uart_init(uart0, 115200);
        gpio_set_function(0, GPIO_FUNC_UART);  // TX
        gpio_set_function(1, GPIO_FUNC_UART);  // RX

        irq_set_exclusive_handler(UART0_IRQ, on_uart_interrupt_0);
        irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
}

UartManagerPico::~UartManagerPico() {
    instance = nullptr;
    uart_set_irq_enables(uart0, false, false);
        irq_set_enabled(UART0_IRQ, false);
    delete channel;
}

void UartManagerPico::sendData(const char* data, size_t length) {
    if (length == 0 || data == nullptr) {
        return;
    }
    uart_write_blocking(uart0, (const uint8_t*)data, length);
}

void UartManagerPico::onInterrupt() {
    channel->sendExternalNoBlock(true);
    }
    
size_t UartManagerPico::read(char* buffer, size_t bufferSize) {
    auto [signal, error] = channel->receive();
    if (error || !signal) return 0;
    size_t index = 0;
    while (uart_is_readable(uart0) && index < bufferSize) {
        buffer[index++] = uart_getc(uart0);
    }
    return index;
}