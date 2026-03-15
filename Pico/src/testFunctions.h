#pragma once

static void uart_write_str(const char* s) {
    uart_write_blocking(uart0, (const uint8_t*)s, strlen(s));
}

void test(){
    initPicoLed();
    uart_init(uart0, 115200);
    gpio_set_function(0, GPIO_FUNC_UART);  // TX
    gpio_set_function(1, GPIO_FUNC_UART);  // RX

    char lineBuf[256];
    int  lineBufPos = 0;
    int  helloIndex = 0;
    uint32_t lastHelloMs = 0;

    while (true) {
        // Print "hello N" every 2 seconds
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastHelloMs >= 2000) {
            char buf[64];
            snprintf(buf, sizeof(buf), "hello %d\r\n", helloIndex++);
            uart_write_str(buf);
            lastHelloMs = now;
        }

        // Non-blocking read
        if (!uart_is_readable(uart0)) continue;
        char c = uart_getc(uart0);

        if (c == '\n' || c == '\r') {
            if (lineBufPos > 0) {
                toggleDefaultLed();
                lineBuf[lineBufPos] = '\0';
                char buf[280];
                snprintf(buf, sizeof(buf), "response:%s\r\n", lineBuf);
                uart_write_str(buf);
                lineBufPos = 0;
            }
        } else if (lineBufPos < (int)sizeof(lineBuf) - 1) {
            lineBuf[lineBufPos++] = c;
        }
    }
}
