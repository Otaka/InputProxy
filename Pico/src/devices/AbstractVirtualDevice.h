#ifndef ABSTRACT_VIRTUAL_DEVICE_H
#define ABSTRACT_VIRTUAL_DEVICE_H

#include <functional>
#include <stdint.h>

// Structure describing a single axis/button control
struct AxisDescription {
    char* name;       // Human-readable name
    int axisIndex;    // The axis code/index to use with setAxis()
};

// Structure describing all axes available in a device
struct AxesDescription {
    AxisDescription* axes;  // Array of axis descriptions
    int axesCount;          // Number of axes in the array
};

class AbstractVirtualDevice {
public:
    virtual ~AbstractVirtualDevice() = default;

    // Set axis value for a control code
    // code: control code (e.g., keycode for keyboard)
    // value: value in range [0...1000]
    virtual void setAxis(int code, int value) = 0;

    // Set callback for events from PC
    // lambda: callback function(int code, int value)
    virtual void setOnEvent(std::function<void(int, int)> lambda) = 0;

    // Initialize the device
    virtual bool init() = 0;

    // Update/process device state (called in main loop)
    virtual void update() = 0;

    // Get description of all axes (buttons and axes)
    // Index in returned array corresponds to axis index
    virtual AxesDescription axesDescription() = 0;
};

#endif // ABSTRACT_VIRTUAL_DEVICE_H
