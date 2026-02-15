#include "tud_driver_xinput.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;

    // Endpoint transfer buffers
    CFG_TUSB_MEM_ALIGN uint8_t ep_in_buf[32];
    CFG_TUSB_MEM_ALIGN uint8_t ep_out_buf[32];

    // Transfer states
    bool ep_in_busy;
} xinput_interface_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

#ifndef MAX_GAMEPADS
#define MAX_GAMEPADS 4
#endif

CFG_TUSB_MEM_SECTION static xinput_interface_t _xinput_itf[MAX_GAMEPADS];
static uint8_t _xinput_itf_count = 0;

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

bool tud_xinput_ready(uint8_t gamepad_index) {
    if (gamepad_index >= MAX_GAMEPADS) return false;
    return !_xinput_itf[gamepad_index].ep_in_busy && tud_ready();
}

bool tud_xinput_report(uint8_t gamepad_index, const xinput_report_t* report) {
    if (!tud_xinput_ready(gamepad_index)) {
        return false;
    }

    // Copy report to endpoint buffer
    memcpy(_xinput_itf[gamepad_index].ep_in_buf, report, sizeof(xinput_report_t));

    // Submit transfer
    _xinput_itf[gamepad_index].ep_in_busy = true;
    return usbd_edpt_xfer(TUD_OPT_RHPORT, _xinput_itf[gamepad_index].ep_in, _xinput_itf[gamepad_index].ep_in_buf, sizeof(xinput_report_t));
}

// Weak implementation - can be overridden by application
TU_ATTR_WEAK void tud_xinput_receive_report_cb(uint8_t gamepad_index, xinput_out_report_t const* report) {
    (void)gamepad_index;
    (void)report;
}

//--------------------------------------------------------------------+
// CLASS DRIVER IMPLEMENTATION
//--------------------------------------------------------------------+

void xinput_init(void) {
    tu_memclr(_xinput_itf, sizeof(_xinput_itf));
    _xinput_itf_count = 0;
}

void xinput_reset(uint8_t rhport) {
    (void)rhport;
    tu_memclr(_xinput_itf, sizeof(_xinput_itf));
    _xinput_itf_count = 0;
}

uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
    TU_VERIFY(itf_desc->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC &&
              itf_desc->bInterfaceSubClass == 0x5D &&
              itf_desc->bInterfaceProtocol == 0x01, 0);

    // Check if we have room for another interface
    TU_VERIFY(_xinput_itf_count < MAX_GAMEPADS, 0);

    uint8_t const itf_index = _xinput_itf_count;
    xinput_interface_t* p_xinput = &_xinput_itf[itf_index];
    _xinput_itf_count++;

    uint16_t drv_len = 0;
    uint8_t const* p_desc = (uint8_t const*)itf_desc;

    // Interface descriptor
    p_xinput->itf_num = itf_desc->bInterfaceNumber;
    drv_len += tu_desc_len(itf_desc);
    p_desc = tu_desc_next(p_desc);

    // Parse endpoints
    uint8_t found_endpoints = 0;
    while (found_endpoints < 2 && drv_len < max_len) {
        tusb_desc_type_t const desc_type = (tusb_desc_type_t)tu_desc_type(p_desc);

        if (desc_type == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)p_desc;

            if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
                p_xinput->ep_in = ep_desc->bEndpointAddress;
                TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
            } else {
                p_xinput->ep_out = ep_desc->bEndpointAddress;
                TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
            }

            found_endpoints++;
            drv_len += tu_desc_len(p_desc);
            p_desc = tu_desc_next(p_desc);
        } else {
            // Skip unknown descriptors
            drv_len += tu_desc_len(p_desc);
            p_desc = tu_desc_next(p_desc);
        }
    }

    // Prepare to receive output reports (rumble)
    if (p_xinput->ep_out) {
        usbd_edpt_xfer(rhport, p_xinput->ep_out, p_xinput->ep_out_buf, sizeof(p_xinput->ep_out_buf));
    }

    return drv_len;
}

bool xinput_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // Handle XInput-specific control requests
    if (stage == CONTROL_STAGE_SETUP) {
        // XInput uses vendor-specific control requests for LED control
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
            // Handle vendor-specific LED/rumble control requests
            if (request->bmRequestType_bit.direction == TUSB_DIR_OUT) {
                // Host is sending data (LED control) - use first interface buffer
                if (request->wLength <= sizeof(_xinput_itf[0].ep_out_buf)) {
                    return tud_control_xfer(rhport, request, _xinput_itf[0].ep_out_buf, request->wLength);
                }
            } else {
                // Host is requesting data
                if (request->bRequest == 0x01) {
                    // XInput capabilities/descriptor request
                    static uint8_t const capabilities[] = {
                        0x00, 0x14,  // Type and size
                        0xFF, 0xFF,  // All buttons supported
                        0xFF, 0xFF, 0xFF, 0xFF,  // Reserved
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Reserved
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Reserved
                    };
                    uint16_t len = request->wLength;
                    if (len > sizeof(capabilities)) len = sizeof(capabilities);
                    return tud_control_xfer(rhport, request, (void*)capabilities, len);
                }
            }
        }
    }

    return false;
}

bool xinput_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)result;

    // Find which interface this endpoint belongs to
    for (uint8_t i = 0; i < _xinput_itf_count; i++) {
        if (ep_addr == _xinput_itf[i].ep_in) {
            // IN transfer complete
            _xinput_itf[i].ep_in_busy = false;
            return true;
        } else if (ep_addr == _xinput_itf[i].ep_out) {
            // OUT transfer complete (rumble data received)
            if (xferred_bytes >= sizeof(xinput_out_report_t)) {
                xinput_out_report_t* report = (xinput_out_report_t*)_xinput_itf[i].ep_out_buf;
                tud_xinput_receive_report_cb(i, report);
            }

            // Prepare for next OUT transfer
            usbd_edpt_xfer(rhport, _xinput_itf[i].ep_out, _xinput_itf[i].ep_out_buf, sizeof(_xinput_itf[i].ep_out_buf));
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------+
// CLASS DRIVER REGISTRATION
//--------------------------------------------------------------------+

// TinyUSB class driver structure
static usbd_class_driver_t const _xinput_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
#endif
    .init = xinput_init,
    .reset = xinput_reset,
    .open = xinput_open,
    .control_xfer_cb = xinput_control_xfer_cb,
    .xfer_cb = xinput_xfer_cb,
    .sof = NULL
};

// Export with C linkage for TinyUSB
extern "C" {
    // This function needs to be called to register the driver
    usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
        *driver_count = 1;
        return &_xinput_driver;
    }
}
