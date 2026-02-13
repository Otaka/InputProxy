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


## Installation
Add these lines to /boot/firmware/config.txt (in the main section, NOT under [all]):

enable_uart=1
dtoverlay=uart2

## Connect PICO to Rasbperry pi 4
Pico 1:
Orange- 6 (Ground)
Red   - 8 (GPIO 14 TXD)
Brown - 10(GPIO 15 RXD)

Pico 2
Orange- 30(Ground)
Red   - 28(GPIO 1)
Brown - 27(GPIO 0)