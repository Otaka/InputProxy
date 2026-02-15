#ifndef XINPUT_DESCRIPTORS_H
#define XINPUT_DESCRIPTORS_H

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// XInput Interface Protocol
#define XINPUT_SUBCLASS 0x5D
#define XINPUT_PROTOCOL 0x01

// XInput Endpoint Size
#define XINPUT_EPSIZE 32

// XInput Descriptor Length: Interface (9) + XInput-specific (17) + EP In (7) + EP Out (7)
#define TUD_XINPUT_DESC_LEN  (9 + 17 + 7 + 7)

/**
 * XInput Interface Descriptor Macro
 *
 * @param _itfnum  Interface number
 * @param _stridx  String descriptor index
 * @param _epin    IN endpoint address (e.g., 0x81)
 * @param _epout   OUT endpoint address (e.g., 0x01)
 *
 * This descriptor includes:
 * - Standard interface descriptor (vendor-specific class)
 * - XInput-specific unknown descriptor (type 0x21, required for Windows driver)
 * - IN endpoint (for gamepad state reports)
 * - OUT endpoint (for rumble/LED commands)
 */
#define TUD_XINPUT_DESCRIPTOR(_itfnum, _stridx, _epin, _epout) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, XINPUT_SUBCLASS, XINPUT_PROTOCOL, _stridx,\
  /* XInput-specific unknown descriptor (type 0x21, required for proper enumeration) */\
  /* This descriptor contains endpoint addresses and max data sizes */\
  17, 0x21, 0x00, 0x01, 0x01, 0x25, _epin, 0x14, 0x00, 0x00, 0x00, 0x00, 0x13, _epout, 0x08, 0x00, 0x00,\
  /* Endpoint In (to host) - gamepad state reports */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(XINPUT_EPSIZE), 4,\
  /* Endpoint Out (from host) - rumble/LED commands */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(XINPUT_EPSIZE), 8

#ifdef __cplusplus
}
#endif

#endif // XINPUT_DESCRIPTORS_H
