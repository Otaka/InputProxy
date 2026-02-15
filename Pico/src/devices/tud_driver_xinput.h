#ifndef TUD_DRIVER_XINPUT_H
#define TUD_DRIVER_XINPUT_H

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of XInput gamepads
#ifndef MAX_GAMEPADS
#define MAX_GAMEPADS 4
#endif

//--------------------------------------------------------------------+
// XInput Report Structures
//--------------------------------------------------------------------+

// XInput Input Report (from device to host) - 20 bytes
// This matches the actual Xbox 360 controller wire format
typedef struct TU_ATTR_PACKED {
    uint8_t report_id;      // 0x00 - Message type
    uint8_t report_size;    // 0x14 - Packet size (20 bytes)
    uint16_t buttons;       // All buttons packed: YXBA | Start,Back,LS,RS | DUp,DDown,DLeft,DRight | LB,RB,Guide
    uint8_t trigger_l;      // Left trigger (0-255)
    uint8_t trigger_r;      // Right trigger (0-255)
    int16_t joystick_lx;    // Left stick X (-32768 to 32767)
    int16_t joystick_ly;    // Left stick Y (-32768 to 32767)
    int16_t joystick_rx;    // Right stick X (-32768 to 32767)
    int16_t joystick_ry;    // Right stick Y (-32768 to 32767)
    uint8_t reserved[6];    // Reserved/unused
} xinput_report_t;

// XInput Output Report (from host to device) - 8 bytes
typedef struct TU_ATTR_PACKED {
    uint8_t report_id;      // 0x00
    uint8_t report_size;    // 0x08
    uint8_t led;            // LED pattern
    uint8_t rumble_l;       // Left motor (large)
    uint8_t rumble_r;       // Right motor (small)
    uint8_t reserved[3];    // Reserved/unused
} xinput_out_report_t;

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if XInput interface is ready to send report
// gamepad_index: 0-3 for which gamepad to check
bool tud_xinput_ready(uint8_t gamepad_index);

// Send XInput report for specific gamepad
// gamepad_index: 0-3 for which gamepad to send
bool tud_xinput_report(uint8_t gamepad_index, const xinput_report_t* report);

// Callback when output report (rumble) is received
// Must be implemented by application
// gamepad_index: 0-3 indicating which gamepad received the report
void tud_xinput_receive_report_cb(uint8_t gamepad_index, xinput_out_report_t const* report);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+

void xinput_init(void);
void xinput_reset(uint8_t rhport);
uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len);
bool xinput_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request);
bool xinput_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif // TUD_DRIVER_XINPUT_H
