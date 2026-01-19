#include "Timer.h"
#include <vector>

Timer::Timer() : next_timer_id_(1) {}

Timer::~Timer() {
    timers_.clear();
    timersToAdd_.clear();
    timersToRemove_.clear();
}

int Timer::setInterval(Callback callback, int timeout_ms) {
    int timer_id = next_timer_id_++;
    
    TimerInfo timer_info;
    timer_info.timer_id = timer_id;
    timer_info.callback = callback;
    timer_info.timeout_ms = timeout_ms;
    timer_info.next_fire_time = make_timeout_time_ms(timeout_ms);
    timer_info.is_interval = true;
    timer_info.active = true;
    
    timersToAdd_.push_back(timer_info);
    
    return timer_id;
}

int Timer::setTimeout(Callback callback, int timeout_ms) {
    int timer_id = next_timer_id_++;
    
    TimerInfo timer_info;
    timer_info.timer_id = timer_id;
    timer_info.callback = callback;
    timer_info.timeout_ms = timeout_ms;
    timer_info.next_fire_time = make_timeout_time_ms(timeout_ms);
    timer_info.is_interval = false;
    timer_info.active = true;
    
    timersToAdd_.push_back(timer_info);
    
    return timer_id;
}

void Timer::cancel(int timer_id) {
    timersToRemove_.push_back(timer_id);
}

void Timer::process() {
    absolute_time_t now = get_absolute_time();
    
    // Process removals
    for (int remove_id : timersToRemove_) {
        for (size_t i = 0; i < timers_.size(); i++) {
            if (timers_[i].timer_id == remove_id) {
                timers_.erase(timers_.begin() + i);
                break;
            }
        }
    }
    timersToRemove_.clear();
    
    // Process additions
    for (const TimerInfo& timer : timersToAdd_) {
        timers_.push_back(timer);
    }
    timersToAdd_.clear();
    
    // Process timers
    for (size_t i = 0; i < timers_.size(); i++) {
        TimerInfo& timer = timers_[i];
        
        // Check if timer should fire
        if (timer.active && absolute_time_diff_us(timer.next_fire_time, now) >= 0) {
            // Execute callback
            if (timer.callback) {
                timer.callback();
            }
            
            // Update timer based on type
            if (timer.is_interval) {
                // Reschedule interval timer
                timer.next_fire_time = make_timeout_time_ms(timer.timeout_ms);
            } else {
                // Remove one-shot timer
                timers_.erase(timers_.begin() + i);
                i--; // Adjust index after removal
            }
        }
    }
}
