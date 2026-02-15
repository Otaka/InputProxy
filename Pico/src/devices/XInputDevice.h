#ifndef XINPUT_DEVICE_H
#define XINPUT_DEVICE_H
#include "../shared/shared.h"
#include "AbstractVirtualDevice.h"
#include "tud_driver_xinput.h"
#include <functional>
#include <stdint.h>
#include <cstring>

// Maximum number of XInput gamepads supported
#ifndef MAX_GAMEPADS
#define MAX_GAMEPADS 4
#endif

class XInputDevice : public AbstractVirtualDevice {
public:
    XInputDevice(uint8_t gamepad_index = 0);
    virtual ~XInputDevice() = default;

    // AbstractVirtualDevice interface implementation
    void setAxis(int code, int value) override;
    void setOnEvent(std::function<void(int, int)> lambda) override;
    bool init() override;
    void update() override;
    AxesDescription axesDescription() override;

    // Get gamepad index
    uint8_t getGamepadIndex() const { return gamepadIndex; }

    // Handle rumble callback from TinyUSB
    void handleRumble(const xinput_out_report_t* report);

private:
    xinput_report_t report;
    uint8_t gamepadIndex;
    bool reportChanged;
    uint32_t lastReportTime;  // For periodic reporting

    // Callback for events from PC (e.g., rumble)
    std::function<void(int, int)> eventCallback;

    // Helper methods
    void setButton(uint8_t button, bool pressed);
    void sendReport();

    // Convert 0-1000 range to 0-255 for triggers
    uint8_t convertToTriggerValue(int value);

    // Convert 0-1000 range to -32768 to 32767 for sticks
    int16_t convertToStickValue(int valueMinus, int valuePlus);
};

#endif // XINPUT_DEVICE_H
