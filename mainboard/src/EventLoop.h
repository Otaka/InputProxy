#pragma once

#include <vector>
#include <functional>
#include <chrono>
#include <mutex>
#include <cstdio>

class EventLoop {
public:
    using Runnable = std::function<void()>;
    using FdHandler = std::function<void(int)>;

    EventLoop();
    ~EventLoop();

    int addOneShot(std::chrono::milliseconds delay, Runnable handler);
    int addRepeatable(std::chrono::milliseconds interval, Runnable handler);
    int addFileDescriptor(int fd, FdHandler onDataHandler, FdHandler onFileRemovedHandler);
    int addFile(FILE* file, FdHandler onDataHandler, FdHandler onFileRemovedHandler);
    void removeFileDescriptor(int fd);
    void invokeLater(Runnable task);
    void stopEventLoop();
    void runLoop();

private:
    struct FdInfo {
        int fd;
        FdHandler onDataHandler;
        FdHandler onFdClosedHandler;
    };

    struct TimerInfo {
        int fd;
        bool repeatable;
        std::chrono::milliseconds interval;
    };

    int scheduledNextIterationEvent;
    std::vector<Runnable> scheduledNextIteration;
    std::vector<FdInfo> fds;
    std::vector<TimerInfo> timers;
    std::mutex mutex;
    bool finish;
};
