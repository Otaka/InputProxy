# InputProxy

A hardware/software system for capturing and emulating USB input devices using Raspberry Pi as a proxy.

## Why InputProxy?

InputProxy provides advanced input transformation capabilities that go beyond standard operating system features:

- **Complex hotkeys**: Support for multi-key combinations (3, 4, 5+ keys)
- **Key sequences**: React to chord patterns and key sequences
- **Press and release events**: Handle both key press and release actions separately
- **Turbo mode**: Rapid-fire any button on any virtual device
- **Cross-device mapping**: Map keyboard to gamepad, gamepad to keyboard, or any combination
- **Profile switching**: Enable different input configurations on the fly
- **Universal remapping**: Remap any input to any output
- **Lua scripting**: Execute custom Lua scripts as event handlers for complex logic

## Overview

InputProxy consists of two main components:

### Mainboard (Raspberry Pi 3 or 4)
- Acts as the computation center and hosts real USB devices (keyboard, mouse, gamepads)
- Uses Linux input subsystem to read device events
- Transforms events to unified 0...1 "axis" events with specified codes
- Routes and transforms events based on JSON configuration

### Pico (Raspberry Pi Pico)
- Emulates USB devices (mouse, keyboard, gamepads)
- Connects to mainboard via UART (supports up to 2 Picos simultaneously)
- Can work only in one operation mode:
  - **HID Mode**: Emulates mouse, keyboard, and generic HID gamepads (configurable buttons/axes)
  - **XINPUT Mode**: Emulates 1-4 Xbox360 gamepads
- Supports bidirectional communication:
  - Force feedback for gamepads
  - LED state changes for keyboards

## Architecture

With dual Pico setup(one Pico works on "HID Mode" and second in "XINPUT Mode"), the system can simultaneously emulate:
- Keyboard and mouse (HID)
- Generic HID gamepads
- Xbox360 gamepads (XInput)

## Status

Work in progress - implementation details are being actively developed.


## Installation (Raspberry Pi 4)

### 1. Enable UARTs in /boot/firmware/config.txt

Add these lines in the main section (NOT under [cm4], [cm5], or [all]):
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
- `/dev/ttyAMA3` (UART0, GPIO 14/15) — also accessible as `/dev/serial0`
- `/dev/ttyAMA4` (UART2, GPIO 0/1)

## Connect PICO to Rasbperry pi 4
Pico wires:
Pin 1: Brown - TXD
Pin 2: Red - RXD
Pin 3: Orange - Ground

Pico 1 → UART3
Pico Red (RXD)   -> Pi GPIO4 (Pin 7)
Pico Brown (TXD) -> Pi GPIO5 (Pin 29)
Pico Orange(GND) -> Pi any Ground(I.E. Pin 6) 

Pico 2 → UART4
Pico Red (RXD)   -> Pi GPIO8 (Pin 24)
Pico Brown (TXD) -> Pi GPIO9 (Pin 21)
Pico Orange(GND) -> Pi any Ground(I.E. Pin 9) 
