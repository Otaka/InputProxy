# InputProxy

A hardware/software system for capturing and emulating USB input devices using Raspberry Pi as a proxy.

## Why InputProxy?

InputProxy provides advanced input transformation capabilities that go beyond standard operating system features:

- **Complex hotkeys**: Support for multi-key combinations (3, 4, 5+ keys)
- **Key sequences**: React to chord patterns and key sequences
- **Press and release events**: Handle both key press and release actions separately
- **Cross-device mapping**: Map keyboard to gamepad, gamepad to keyboard, or any combination
- **Profile switching**: Enable different input configurations on the fly via layers
- **Universal remapping**: Remap any input to any output
- **Output sequences**: Emit complex button sequences with timing delays as a single action

## Overview

InputProxy consists of two main components:

### Mainboard (Raspberry Pi 3 or 4)
- Acts as the computation center and hosts real USB devices (keyboard, mouse, gamepads)
- Uses Linux input subsystem to read device events
- Transforms events to unified 0...1000 "axis" events with specified codes
- Routes and transforms events based on JSON configuration
- Runs an HTTP REST API on port 8080 for runtime control

### Pico (Raspberry Pi Pico)
- Emulates USB devices (mouse, keyboard, gamepads)
- Connects to mainboard via UART (supports up to 2 Picos simultaneously)
- Operates in one of two modes:
  - **HID Mode**: Emulates mouse, keyboard, and generic HID gamepads (configurable buttons/axes)
  - **XINPUT Mode**: Emulates 1-4 Xbox 360 gamepads
- Supports bidirectional communication:
  - Force feedback for gamepads
  - LED state changes for keyboards

## Architecture

```
Real USB Devices        Raspberry Pi 4         Pico (UART)
─────────────────       ──────────────────     ──────────────────────
Xbox 360 Gamepad  ───►  RealDeviceManager  ───► HID Mode:
PS4 Controller    ───►  MappingManager     ───►   Keyboard + Mouse
Generic Gamepad   ───►  LayerManager       ───►   HID Gamepads
Keyboard          ───►  REST API (:8080)   ───► XINPUT Mode:
Mouse             ───►                    ───►   Xbox 360 Gamepad(s)
```

With a dual Pico setup (one in HID Mode, one in XINPUT Mode), the system can simultaneously emulate:
- Keyboard and mouse (HID)
- Generic HID gamepads
- Xbox 360 gamepads (XInput)

---

## Installation (Raspberry Pi 4)

### 1. Enable UARTs in /boot/firmware/config.txt

Add these lines in the main section (NOT under `[cm4]`, `[cm5]`, or `[all]`):
```
enable_uart=1
dtoverlay=uart3
dtoverlay=uart4
```

### 2. Disable serial console

By default Raspbian uses UART0 as a login console. This must be disabled:
```bash
sudo systemctl stop serial-getty@ttyS0.service
sudo systemctl disable serial-getty@ttyS0.service
```

Remove `console=serial0,115200` from `/boot/firmware/cmdline.txt`:
```bash
sudo sed -i 's/console=serial0,[0-9]* //g' /boot/firmware/cmdline.txt
```

### 3. Reboot

After reboot, the following devices should be available:
- `/dev/ttyAMA3` (UART3, GPIO 14/15) — also accessible as `/dev/serial0`
- `/dev/ttyAMA4` (UART4, GPIO 0/1)

## Connect Pico to Raspberry Pi 4

Pico wires:
- Pin 1: Brown - TXD
- Pin 2: Red - RXD
- Pin 3: Orange - Ground

```
Pico 1 → UART3
  Pico Red (RXD)    → Pi GPIO4 (Pin 7)
  Pico Brown (TXD)  → Pi GPIO5 (Pin 29)
  Pico Orange (GND) → Pi any Ground (e.g. Pin 6)

Pico 2 → UART4
  Pico Red (RXD)    → Pi GPIO8 (Pin 24)
  Pico Brown (TXD)  → Pi GPIO9 (Pin 21)
  Pico Orange (GND) → Pi any Ground (e.g. Pin 9)
```

---

## Configuration File

The system is configured via `config.json` (placed next to the mainboard binary). A full config has four top-level sections:

```json
{
    "emulation_boards": [...],
    "virtual_input_devices": [...],
    "real_devices": [...],
    "layers": [...]
}
```

The REST API endpoint `POST /config/reload` applies a new config without restarting.

---

### Section 1: `emulation_boards`

Defines the virtual devices the Pico will emulate. Each board maps to one physical Pico.

```json
"emulation_boards": [
    {
        "id": "PICO_SERIAL",
        "vid": 4618,
        "pid": 4,
        "manufacturer": "InputProxy",
        "product": "InputProxy Composite1",
        "serial": "20260107",
        "devices": [
            { "id": "vkbd1",  "type": "keyboard",      "name": "InputProxy Keyboard" },
            { "id": "vmouse", "type": "mouse",          "name": "InputProxy Mouse" },
            { "id": "vgp1",   "type": "hidgamepad",     "name": "InputProxy Gamepad 1",
              "buttons": 13, "axesmask": 31, "hat": true },
            { "id": "vgp2",   "type": "hidgamepad",     "name": "InputProxy Gamepad 2",
              "buttons": 13, "axesmask": 31, "hat": true }
        ]
    }
]
```

The `id` field must match the serial string the Pico reports at boot. The system verifies this to send the correct configuration.

#### Device types

| `type`           | Description                              |
|------------------|------------------------------------------|
| `keyboard`       | HID keyboard                             |
| `mouse`          | HID mouse                                |
| `hidgamepad`     | Generic HID gamepad (configurable)       |
| `xbox360gamepad` | Xbox 360 controller (XInput)             |

> **Note:** If any device in the board has type `xbox360gamepad`, the entire board switches to XINPUT mode and its USB VID/PID are overridden to Microsoft's Xbox 360 values (045E:028E). XINPUT and HID devices cannot coexist on the same Pico.

#### HID Gamepad parameters

| Field       | Description                                          |
|-------------|------------------------------------------------------|
| `buttons`   | Number of buttons (1–32)                             |
| `axesmask`  | Bitmask of enabled analog axes (see table below)     |
| `hat`       | `true` to enable D-pad / hat switch                  |

`axesmask` bit values:
- Bit 0 (1): Left stick X/Y
- Bit 1 (2): Left stick Z
- Bit 2 (4): Right stick X/Y
- Bit 3 (8): Right stick Z
- Bit 4 (16): Dial/Slider

Use `"axesmask": 31` to enable all axes.

#### Setting up an Xbox 360 gamepad output (config_xbox.json example)

```json
"emulation_boards": [
    {
        "id": "7I2GP",
        "vid": 4618,
        "pid": 4,
        "manufacturer": "InputProxy",
        "product": "InputProxy Composite1",
        "serial": "20260107",
        "devices": [
            { "id": "vxbx1", "type": "xbox360gamepad", "name": "InputProxy Gamepad 2" }
        ]
    }
]
```

The Xbox 360 output device (`vxbx1`) has these named axes for mapping targets:

```
Dpad_Up, Dpad_Down, Dpad_Left, Dpad_Right
Button_Start, Button_Back
Button_Stick_Left, Button_Stick_Right
Button_Bumper_Left, Button_Bumper_Right
Button_XBOX
Button_A, Button_B, Button_X, Button_Y
Button_Trigger_Left, Button_Trigger_Right
Stick_Left_X-, Stick_Left_X+, Stick_Left_Y+, Stick_Left_Y-
Stick_Right_X-, Stick_Right_X+, Stick_Right_Y+, Stick_Right_Y-
```

#### Setting up a generic HID gamepad output

The HID gamepad output device uses these axis names for mapping targets:

```
Dpad_Up, Dpad_Down, Dpad_Left, Dpad_Right
Button_1 … Button_32
Stick_Left_X-, Stick_Left_X+, Stick_Left_Y-, Stick_Left_Y+
Stick_Left_Z-, Stick_Left_Z+
Stick_Right_X-, Stick_Right_X+, Stick_Right_Y-, Stick_Right_Y+
Stick_Right_Z-, Stick_Right_Z+
Dial-, Dial+, Slider-, Slider+
```

---

### Section 2: `virtual_input_devices`

Virtual input devices (VIDs) are internal slots that real physical devices are assigned to. This decouples the physical device identity from the mapping rules — the same mapping works regardless of which physical device is plugged in, as long as it is assigned to the same VID slot.

```json
"virtual_input_devices": [
    { "id": "vid1", "name": "Gamepad 1" },
    { "id": "vid2", "name": "Gamepad 2" },
    { "id": "vid3", "name": "Gamepad 3" }
]
```

---

### Section 3: `real_devices`

Defines which physical devices to recognize, which VID slot to assign them to, and optional axis renames.

```json
"real_devices": [
    {
        "id": "45e:28e:272:Microsoft X-Box 360 pad:31",
        "assignedTo": "vid1",
        "rename_axes": {
            "Button 304": "ButtonA",
            "Button 305": "ButtonB",
            "Button 307": "ButtonX",
            "Button 308": "ButtonY",
            "Button 310": "ButtonBumperLeft",
            "Button 311": "ButtonBumperRight",
            "Button 314": "ButtonBack",
            "Button 315": "ButtonStart",
            "Button 316": "ButtonXBOX",
            "Button 317": "ButtonThumbLeft",
            "Button 318": "ButtonThumbRight",
            "Stick RZ": "TriggerRight",
            "Stick LZ": "TriggerLeft"
        }
    }
]
```

#### Device ID format

The device ID string is generated automatically from the Linux evdev info:

```
<vendor_hex>:<product_hex>:<version>:<device_name>:<axis_count>
```

Examples:
- `45e:28e:272:Microsoft X-Box 360 pad:31` — Xbox 360 wired controller
- `54c:9cc:33041:Sony Interactive Entertainment Wireless Controller:33` — DualShock 4
- `2dc8:200a:512:Generic X-Box pad:31` — 8BitDo / generic XInput pad

#### Finding the device ID

Connect the device and use the REST API to discover its ID:

```bash
# List all connected real devices
curl http://raspberrypi.local:8080/realdevices/list

# Get detailed info including all axis names for a specific device
curl http://raspberrypi.local:8080/realdevices/detailed/<deviceId>

# Get raw (pre-rename) axis names
curl http://raspberrypi.local:8080/realdevices/detailed/<deviceId>/original-axes
```

The `deviceIdStr` field in the response is what goes into the config `id` field.

#### Raw axis names (before renaming)

The Linux input subsystem reports axes with these default names:

| Linux name   | Meaning                            |
|--------------|------------------------------------|
| `Stick LX`   | Left stick horizontal (ABS_X)      |
| `Stick LY`   | Left stick vertical (ABS_Y)        |
| `Stick LZ`   | Left stick Z / left trigger        |
| `Stick RX`   | Right stick horizontal (ABS_RX)    |
| `Stick RY`   | Right stick vertical (ABS_RY)      |
| `Stick RZ`   | Right stick Z / right trigger      |
| `Hat X`      | D-pad horizontal (ABS_HAT0X)       |
| `Hat Y`      | D-pad vertical (ABS_HAT0Y)         |
| `Button NNN` | Any key/button (e.g. `Button 304`) |
| `X Axis`     | Mouse X relative movement          |
| `Y Axis`     | Mouse Y relative movement          |
| `Wheel`      | Mouse scroll wheel                 |

Centered axes (sticks, triggers) are automatically split into two directional axes:
- `Stick LX` → `Stick LX+` (right) and `Stick LX-` (left)
- `Stick LY` → `Stick LY+` (down) and `Stick LY-` (up)
- etc.

#### Xbox 360 controller axis renames

The standard Xbox 360 wired controller (`45e:28e`) uses these raw evdev codes:

```json
"rename_axes": {
    "Button 304": "ButtonA",
    "Button 305": "ButtonB",
    "Button 307": "ButtonX",
    "Button 308": "ButtonY",
    "Button 310": "ButtonBumperLeft",
    "Button 311": "ButtonBumperRight",
    "Button 314": "ButtonBack",
    "Button 315": "ButtonStart",
    "Button 316": "ButtonXBOX",
    "Button 317": "ButtonThumbLeft",
    "Button 318": "ButtonThumbRight",
    "Stick RZ":   "TriggerRight",
    "Stick LZ":   "TriggerLeft"
}
```

#### DualShock 4 axis renames

```json
"rename_axes": {
    "Button 304": "ButtonA",
    "Button 305": "ButtonB",
    "Button 308": "ButtonX",
    "Button 307": "ButtonY",
    "Button 310": "ButtonBumperLeft",
    "Button 311": "ButtonBumperRight",
    "Button 312": "ButtonTriggerLeft",
    "Button 313": "ButtonTriggerRight",
    "Button 314": "ButtonBack",
    "Button 315": "ButtonStart",
    "Button 317": "ButtonThumbLeft",
    "Button 318": "ButtonThumbRight",
    "Stick RZ":   "TriggerRight",
    "Stick LZ":   "TriggerLeft"
}
```

---

### Section 4: `layers`

Layers contain the mapping rules. Multiple layers can be defined. Each layer can be active or inactive. When multiple layers are active, they are processed in stack order — a rule that consumes an event (non-propagating) stops processing in lower layers.

```json
"layers": [
    {
        "id": "base",
        "name": "Base Layer",
        "active": true,
        "rules": [...]
    }
]
```

Layers can be toggled at runtime via the REST API:
```bash
curl -X POST http://raspberrypi.local:8080/layers/base/activate
curl -X POST http://raspberrypi.local:8080/layers/base/deactivate
curl http://raspberrypi.local:8080/layers
curl http://raspberrypi.local:8080/layers/active
```

---

## Mapping Rules

### Simple Mapping

Simple rules pass axis values 1:1 from a virtual input device (VID) to a virtual output device (VOD). This is the most common rule type for basic remapping.

```json
{
    "type": "simple",
    "vid": "vid1",
    "vod": "vxbx1",
    "axes": [
        { "from": "ButtonA",           "to": "Button_A" },
        { "from": "ButtonB",           "to": "Button_B" },
        { "from": "ButtonX",           "to": "Button_X" },
        { "from": "ButtonY",           "to": "Button_Y" },
        { "from": "ButtonBumperLeft",  "to": "Button_Bumper_Left" },
        { "from": "ButtonBumperRight", "to": "Button_Bumper_Right" },
        { "from": "ButtonBack",        "to": "Button_Back" },
        { "from": "ButtonStart",       "to": "Button_Start" },
        { "from": "ButtonXBOX",        "to": "Button_XBOX" },
        { "from": "ButtonThumbLeft",   "to": "Button_Stick_Left" },
        { "from": "ButtonThumbRight",  "to": "Button_Stick_Right" },
        { "from": "TriggerLeft",       "to": "Button_Trigger_Left" },
        { "from": "TriggerRight",      "to": "Button_Trigger_Right" },
        { "from": "Stick LX-",         "to": "Stick_Left_X-" },
        { "from": "Stick LX+",         "to": "Stick_Left_X+" },
        { "from": "Stick LY-",         "to": "Stick_Left_Y-" },
        { "from": "Stick LY+",         "to": "Stick_Left_Y+" },
        { "from": "Stick RX-",         "to": "Stick_Right_X-" },
        { "from": "Stick RX+",         "to": "Stick_Right_X+" },
        { "from": "Stick RY-",         "to": "Stick_Right_Y-" },
        { "from": "Stick RY+",         "to": "Stick_Right_Y+" },
        { "from": "Hat Y-",            "to": "Dpad_Up" },
        { "from": "Hat Y+",            "to": "Dpad_Down" },
        { "from": "Hat X-",            "to": "Dpad_Left" },
        { "from": "Hat X+",            "to": "Dpad_Right" }
    ]
}
```

- `vid` — the virtual input device ID (source)
- `vod` — the virtual output device ID (destination, defined in `emulation_boards`)
- Each `from` name must match an axis name on the VID (after `rename_axes` is applied)
- Each `to` name must be a valid axis name for the target output device type

---

### Hotkey Mapping

Hotkey rules fire when a specific combination or sequence of buttons is pressed. They support separate `press_action` and `release_action` lists.

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "<hotkey_expression>",
    "propagate": false,
    "press_action": [...],
    "release_action": [...]
}
```

- `vid` — default VID for axis references in the hotkey expression
- `hotkey` — expression describing the trigger (see syntax below)
- `propagate` — if `false` (default), a matched hotkey consumes the event and prevents lower-priority rules from firing; if `true`, other rules still see the event
- `press_action` — actions to run when the hotkey fires (on press)
- `release_action` — actions to run when the last key in the hotkey is released

#### Hotkey expression syntax

**Single button:**
```
ButtonA
```

**Combination (all must be held):**
```
!ButtonBumperLeft+ButtonA
```
`!` marks a modifier — it must already be held before the activation key is pressed. The last token without `!` is the activation axis.

**Multi-device combination:**
```
!vid1:ButtonBumperLeft+vid2:ButtonA
```
Use `vidId:axisName` to reference axes on a different VID than the rule's `vid`.

**Sequence (chord — press one then another):**
```
ButtonBack->ButtonA
```
The `->` separator chains hotkey parts into a sequence. The first part must fire before the second is evaluated.

**Complex example — modifier + sequence:**
```
!ButtonBumperLeft+ButtonBack->ButtonA
```
Hold BumperLeft, press Back, then press A.

#### Action types

**`emit_axis`** — set an axis on an output device:
```json
{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_A" }
```

**`output_sequence`** — emit a sequence of axis changes with optional delays:
```json
{ "type": "output_sequence", "vod": "vxbx1", "sequence": "Button_A:1,50,Button_A:0" }
```
Sequence format: comma-separated tokens of either `AxisName:value` or `NNNms` (sleep).

**`sleep`** — wait before next action:
```json
{ "type": "sleep", "time": 100 }
```

---

### Hotkey examples

#### Remap a single button to a different output

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "ButtonA",
    "propagate": false,
    "press_action":   [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_B" }],
    "release_action": [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_B" }]
}
```

#### Hold modifier + button to trigger a different action

Hold Left Bumper and press A to send Start:

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "!ButtonBumperLeft+ButtonA",
    "propagate": false,
    "press_action":   [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_Start" }],
    "release_action": [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_Start" }]
}
```

#### Press performs one action, release performs another

Press A to press B on output; release A to also release B:

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "ButtonA",
    "propagate": false,
    "press_action":   [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_B" }],
    "release_action": [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_B" }]
}
```

#### Press emits an output sequence (e.g. rapid-fire or macro)

Press A once to automatically press and release B five times:

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "ButtonA",
    "propagate": false,
    "press_action": [{
        "type": "output_sequence",
        "vod": "vxbx1",
        "sequence": "Button_B:1000,50ms,Button_B:0,50ms,Button_B:1000,50ms,Button_B:0,50ms,Button_B:1000,50ms,Button_B:0"
    }],
    "release_action": []
}
```

#### Press triggers one output, release triggers a different output

Useful for actions that need a specific output state on both press and release:

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "ButtonBack",
    "propagate": false,
    "press_action":   [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_A" }],
    "release_action": [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_B" }]
}
```

#### Sequence chord: press Back then A to send Xbox button

```json
{
    "type": "hotkey",
    "vid": "vid1",
    "hotkey": "ButtonBack->ButtonA",
    "propagate": false,
    "press_action":   [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_XBOX" }],
    "release_action": [{ "type": "emit_axis", "vod": "vxbx1", "axis": "Button_XBOX" }]
}
```

---

## REST API Reference

The mainboard exposes an HTTP API on port **8080**.

### Real devices

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/realdevices/list` | List all connected real devices |
| `GET` | `/realdevices/detailed/{deviceId}` | Get device details including all axes |
| `GET` | `/realdevices/detailed/{deviceId}/original-axes` | Get raw axis names (before rename_axes) |

### Emulation boards (Pico)

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/emulationboard/list` | List all registered Pico boards |
| `GET` | `/emulationboard/{id}` | Get board info |
| `GET` | `/emulationboard/{id}/devices` | List emulated devices on a board |
| `POST` | `/emulationboard/{id}/reboot` | Reboot the Pico (`?flash=true` for flash mode) |
| `POST` | `/emulationboard/{id}/ping?value=N` | Ping the Pico |
| `GET` | `/emulationboard/{id}/led` | Get Pico onboard LED state |
| `POST` | `/emulationboard/{id}/led?value=true` | Set Pico onboard LED |
| `POST` | `/emulationboard/{id}/setaxis?device=N&axis=N&value=N` | Directly set an axis value |

### Layers

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/layers` | List all layers with active status |
| `GET` | `/layers/active` | List currently active layers |
| `POST` | `/layers/{id}/activate` | Activate a layer |
| `POST` | `/layers/{id}/deactivate` | Deactivate a layer |

### Config

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/config/reload` | Reload `config.json` without restart |
