#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>

enum UART_CHANNEL {
    UART0,
    UART1,
    UART2,
    UART3,
    UART4,
    UART5
};

class UartManager {
  private:
    std::vector<std::string> devicePaths;
    std::string activeDevicePath;
    int uartFileHandle;
    char buffer[10*1024];

  public:
    UartManager(UART_CHANNEL channel = UART0) : uartFileHandle(-1) {
        switch(channel) {
            case UART0:
                devicePaths = {"/dev/serial0", "/dev/ttyAMA0", "/dev/ttyS0"};
                break;
            case UART1:
                devicePaths = {"/dev/serial1", "/dev/ttyAMA1", "/dev/ttyS1"};
                break;
            case UART2:
                devicePaths = {"/dev/serial2", "/dev/ttyAMA2", "/dev/ttyS2"};
                break;
            case UART3:
                devicePaths = {"/dev/serial3", "/dev/ttyAMA3", "/dev/ttyS3"};
                break;
            case UART4:
                devicePaths = {"/dev/serial4", "/dev/ttyAMA4", "/dev/ttyS4"};
                break;
            case UART5:
                devicePaths = {"/dev/serial5", "/dev/ttyAMA5", "/dev/ttyS5"};
                break;
            default:
                throw std::invalid_argument("Unknown UART channel passed in constructor");
        }
    }

    int getUartFd() const {
        return uartFileHandle;
    }

    const std::string& getActiveDevicePath() const {
        return activeDevicePath;
    }

    bool testHasUartDevice() {
        for (const auto& path : devicePaths) {
            int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
            if (fd >= 0) {
                close(fd);
                activeDevicePath = path;
                return true;
            }
        }
        return false;
    }

    bool configureUart() {
        // Try each device path until one succeeds
        for (const auto& path : devicePaths) {
            uartFileHandle = open(path.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
            if (uartFileHandle >= 0) {
                activeDevicePath = path;
                std::cout << "Successfully opened UART at " << path << std::endl;
                break;
            }
        }

        if (uartFileHandle < 0) {
            std::cerr << "Error opening UART. Tried paths: ";
            for (size_t i = 0; i < devicePaths.size(); ++i) {
                std::cerr << devicePaths[i];
                if (i < devicePaths.size() - 1) std::cerr << ", ";
            }
            std::cerr << std::endl;
            return false;
        }

        struct termios tty;
        if (tcgetattr(uartFileHandle, &tty) != 0) {
            std::cerr << "Error getting terminal attributes: " << strerror(errno) << std::endl;
            close(uartFileHandle);
            uartFileHandle = -1;
            return false;
        }

        speed_t speed=B115200;
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);

        // 8N1 mode
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
        tty.c_iflag &= ~IGNBRK;  // disable break processing
        tty.c_lflag = 0;  // no signaling chars, no echo, no canonical processing
        tty.c_oflag = 0;  // no remapping, no delays
        tty.c_cc[VMIN]  = 0;  // read doesn't block
        tty.c_cc[VTIME] = 5;  // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // shut off xon/xoff ctrl
        tty.c_cflag |= (CLOCAL | CREAD);  // ignore modem controls, enable reading
        tty.c_cflag &= ~(PARENB | PARODD);  // shut off parity
        tty.c_cflag &= ~CSTOPB;  // 1 stop bit
        tty.c_cflag &= ~CRTSCTS;  // no hardware flow control

        if (tcsetattr(uartFileHandle, TCSANOW, &tty) != 0) {
            std::cerr << "Error setting terminal attributes: " << strerror(errno) << std::endl;
            close(uartFileHandle);
            uartFileHandle = -1;
            return false;
        }

        return true;
    }

    // Read raw bytes from UART (non-blocking)
    // Returns number of bytes read (0 if no data available, -1 on error)
    ssize_t uartRead(char* outBuffer, size_t maxLength) {
        if (uartFileHandle < 0) {
            return -1;
        }

        ssize_t bytesRead = read(uartFileHandle, outBuffer, maxLength);
        return bytesRead;
    }

    // Send raw bytes to UART
    // Returns number of bytes written (-1 on error)
    ssize_t uartSend(const char* data, size_t len) {
        if (uartFileHandle < 0) {
            return -1;
        }
        
        ssize_t written = write(uartFileHandle, data, len);
        if (written < 0) {
            std::cerr << "Error writing to UART: " << strerror(errno) << std::endl;
        } else if (static_cast<size_t>(written) != len) {
            std::cerr << "Warning: Only wrote " << written << " of " << len << " bytes" << std::endl;
        }
        return written;
    }

    void flushInput() {
        if (uartFileHandle < 0) {
            return;
        }
        
        tcflush(uartFileHandle, TCIFLUSH);
    }
};