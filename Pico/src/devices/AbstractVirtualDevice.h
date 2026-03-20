#ifndef ABSTRACT_VIRTUAL_DEVICE_H
#define ABSTRACT_VIRTUAL_DEVICE_H

#include <functional>
#include <string>
#include <stdint.h>

// Device type identifier – carried by each device so managers don't need
// the caller to pass it separately.
enum class DeviceType {
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    XINPUT_GAMEPAD
};

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

    void setInterfaceNum(uint8_t n) { m_interfaceNum = n; }
    uint8_t getInterfaceNum() const { return m_interfaceNum; }

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

    // Self-describing device identity (used by managers via plugDevice)
    virtual std::string getName() const = 0;
    virtual DeviceType getDeviceType() const = 0;

protected:
    uint8_t m_interfaceNum = 0;
};

#endif // ABSTRACT_VIRTUAL_DEVICE_H
