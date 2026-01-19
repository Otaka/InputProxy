#ifndef TIMER_H
#define TIMER_H

#include <functional>
#include <vector>
#include "pico/stdlib.h"

class Timer {
public:
    using Callback = std::function<void()>;

    Timer();
    ~Timer();
    
    // Set a repeating timer
    int setInterval(Callback callback, int timeout_ms);
    
    // Set a one-shot timer
    int setTimeout(Callback callback, int timeout_ms);
    
    // Cancel a timer by ID
    void cancel(int timer_id);
    
    // Process timers - call this in your main loop
    void process();

private:
    
    struct TimerInfo {
        int timer_id;
        Callback callback;
        int timeout_ms;
        absolute_time_t next_fire_time;
        bool is_interval;
        bool active;
    };
    
    std::vector<TimerInfo> timers_;
    std::vector<TimerInfo> timersToAdd_;
    std::vector<int> timersToRemove_;
    int next_timer_id_;
};

#endif // TIMER_H
