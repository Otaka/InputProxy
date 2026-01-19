#include "EventLoop.h"
#include <unistd.h>
#include <algorithm>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <poll.h>

EventLoop::EventLoop() {
    scheduledNextIterationEvent = eventfd(0, EFD_NONBLOCK);
    addFileDescriptor(scheduledNextIterationEvent, [this](int) {
        uint64_t val;
        read(scheduledNextIterationEvent, &val, sizeof(val));

        std::vector<Runnable> tasks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.swap(scheduledNextIteration);
        }

        for (auto& task : tasks) {
            task();
        }
        }, [](int) {});
    finish = false;
}

EventLoop::~EventLoop() {
    stopEventLoop();
    ::close(scheduledNextIterationEvent);
}

int EventLoop::addOneShot(std::chrono::milliseconds delay, Runnable handler) {
    int timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    itimerspec its = {};
    its.it_value.tv_sec = delay.count() / 1000;
    its.it_value.tv_nsec = (delay.count() % 1000) * 1000000;
    timerfd_settime(timerFd, 0, &its, nullptr);

    timers.push_back({ timerFd, false, delay });
    addFileDescriptor(timerFd, [this, handler, timerFd](int) {
        uint64_t exp;
        read(timerFd, &exp, sizeof(exp));
        handler();
        removeFileDescriptor(timerFd);
        close(timerFd);
        }, [](int) {});
    return timerFd;
}

int EventLoop::addRepeatable(std::chrono::milliseconds interval, Runnable handler) {
    int timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    itimerspec its = {};
    its.it_value.tv_sec = interval.count() / 1000;
    its.it_value.tv_nsec = (interval.count() % 1000) * 1000000;
    its.it_interval = its.it_value;
    timerfd_settime(timerFd, 0, &its, nullptr);

    timers.push_back({ timerFd, true, interval });
    addFileDescriptor(timerFd, [handler](int fd) {
        uint64_t exp;
        read(fd, &exp, sizeof(exp));
        handler();
        }, [](int) {});
    return timerFd;
}

int EventLoop::addFileDescriptor(int fd, FdHandler onDataHandler, FdHandler onFileRemovedHandler) {
    std::lock_guard<std::mutex> lock(mutex);
    fds.push_back({ fd, std::move(onDataHandler), std::move(onFileRemovedHandler) });
    return fd;
}

int EventLoop::addFile(FILE* file, FdHandler onDataHandler, FdHandler onFileRemovedHandler) {
    int fd = fileno(file);
    return addFileDescriptor(fd, onDataHandler, onFileRemovedHandler);
}

void EventLoop::removeFileDescriptor(int fd) {
    std::lock_guard<std::mutex> lock(mutex);
    fds.erase(
        std::remove_if(fds.begin(), fds.end(),
            [fd](const FdInfo& info) { return info.fd == fd; }),
        fds.end()
    );
}

void EventLoop::invokeLater(Runnable task) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        scheduledNextIteration.push_back(std::move(task));
    }
    uint64_t val = 1;
    write(scheduledNextIterationEvent, &val, sizeof(val));
}

void EventLoop::stopEventLoop() {
    invokeLater([this]() { finish = true; });
}

void EventLoop::runLoop() {
    while (!finish) {
        std::vector<pollfd> pollFds;
        std::vector<FdInfo> fdsCopy;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& fdInfo : fds) {
                pollFds.push_back({ fdInfo.fd, POLLIN, 0 });
            }
            fdsCopy = fds;
        }

        if (poll(pollFds.data(), pollFds.size(), -1) > 0) {
            for (size_t i = 0; i < pollFds.size(); ++i) {
                if (pollFds[i].revents & POLLIN) {
                    fdsCopy[i].onDataHandler(fdsCopy[i].fd);
                }
                else if (pollFds[i].revents & POLLHUP) {
                    fdsCopy[i].onFdClosedHandler(fdsCopy[i].fd);
                    removeFileDescriptor(fdsCopy[i].fd);
                }
            }
        }
    }
}