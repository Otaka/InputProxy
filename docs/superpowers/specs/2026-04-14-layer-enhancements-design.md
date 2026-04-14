# Layer Enhancements Design

**Date:** 2026-04-14  
**Status:** Approved

---

## Overview

This spec extends the existing layer system with four capabilities:

1. **`block` rule** — suppress a VID axis and hold it at a fixed value
2. **`vod_state` rule** — declaratively set a virtual output device to active, silenced, or disconnected
3. **`turbo` rule** — auto-toggle a VID axis at a configurable rate
4. **Layer activation triggers** — hotkey-driven layer switching via `toggle`, `while_active`, and `while_not_active` modes

All new behavior is expressed as rule types inside the existing `rules[]` array. No new top-level config sections. The system is **declarative**: the active state of the system at any moment is derived entirely from which layers are active and what their rules say — not from a history of commands.

---

## Layer Priority

Layers are processed in the order they appear in the `layers[]` array. **Index 0 is highest priority.** When multiple active layers conflict (e.g. two `vod_state` rules for the same VOD, or a `block` and a `simple` for the same axis), the first active layer in array order wins.

---

## New Rule Types

### 1. `block`

Suppresses one or more VID axes and holds them at a fixed value. Consumes the event — no lower-priority rule will see it. If the rule has "value" <>0 for some axis and then we disable the layer - rule sends value "0" to "unpress" the axis.

```json
{
    "type": "block",
    "vid": "vid1",
    "axes": [
        { "axis": "ButtonA", "value": 0 },
        { "axis": "Stick LX-", "value": 500 },
        { "axis": "Stick LX+", "value": 500 }
    ]
}
```

| Field  | Type   | Required | Description |
|--------|--------|----------|-------------|
| `vid`  | string | yes      | Virtual input device ID |
| `axes` | array  | yes      | List of axes to block |
| `axes[].axis` | string | yes | Axis name on the VID (after `rename_axes`) |
| `axes[].value` | int | yes | Value to hold the axis at. `0` = released, `500` = center, `1000` = fully pressed |

**Use cases:**
- Lock a stick to center while a layer is active: `value: 500`
- Force a button to released: `value: 0`
- Force a button to held: `value: 1000`

---

### 2. `vod_state`

Declares the state of a virtual output device. When multiple active layers declare `vod_state` for the same VOD, the highest-priority layer (lowest array index) wins.

```json
{
    "type": "vod_state",
    "vod": "vgp1",
    "state": "silenced"
}
```

| Field   | Type   | Required | Description |
|---------|--------|----------|-------------|
| `vod`   | string | yes      | Virtual output device ID |
| `state` | string | yes      | `"active"`, `"silenced"`, or `"disconnected"` |

**States:**

| State | Behavior |
|-------|----------|
| `active` | Normal operation. Default when no `vod_state` rule applies. |
| `silenced` | All axis output to this VOD is suppressed. USB device stays connected and visible to the host OS. Instant — no reboot required. |
| `disconnected` | Triggers a Pico reboot cycle. The USB device is physically removed from the host OS. Takes several seconds. Use for games/apps that must not see the device at all. |

---

### 3. `turbo`

Auto-toggles a VID axis between `max_value` and `min_value` at a configurable rate. Operates at the VID level — the toggled value is then forwarded by whatever `simple` or `hotkey` rules exist for that axis. No `vod` or `to` field needed. disabling layer/rule in any phase of cycling toggle should set min_value for the axis to "unpress" it.

```json
{
    "type": "turbo",
    "vid": "vid1",
    "axis": "ButtonA",
    "on_ms": 80,
    "off_ms": 40,
    "initial_delay_ms": 150,
    "max_value": 1000,
    "min_value": 0,
    "condition": "while_axis_active"
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `vid` | string | yes | — | Virtual input device ID |
| `axis` | string | yes | — | Axis name on the VID to toggle |
| `on_ms` | int | yes | — | Duration in ms the axis is held at `max_value` |
| `off_ms` | int | yes | — | Duration in ms the axis is held at `min_value` |
| `initial_delay_ms` | int | no | `0` | Wait before the first toggle begins. Prevents a normal short press from entering turbo immediately. |
| `max_value` | int | no | `1000` | High value of the toggle cycle |
| `min_value` | int | no | `0` | Low value of the toggle cycle |
| `condition` | string | yes | — | `"while_axis_active"` or `"always"` |

**Conditions:**

| Condition | Behavior |
|-----------|----------|
| `while_axis_active` | Turbo runs while the VID axis is non-zero (user holds the button/stick). When user releases, axis is set to `min_value` and turbo stops. |
| `always` | Turbo runs continuously while the layer is active, regardless of user input. |

**How it interacts with other rules:**  
The turbo rule intercepts the axis and injects synthetic toggle events into the VID. Downstream `simple` or `hotkey` rules see these toggled values and forward them to the VOD as normal. Within a single layer, rules are evaluated in array order — if a `block` rule appears before a `turbo` rule for the same axis, the block wins. A `block` in a higher-priority (lower index) layer always suppresses turbo from a lower layer.

---

## Layer Activation Triggers

Each layer may optionally include an `activation` block. Without it, the layer is controlled only via the REST API (existing behavior).

```json
{
    "id": "turbo-fire",
    "name": "Turbo Fire",
    "active": false,
    "activation": {
        "mode": "while_active",
        "vid": "vid1",
        "hotkey": "ButtonBumperRight"
    },
    "rules": [...]
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `mode` | string | yes | `"toggle"`, `"while_active"`, or `"while_not_active"` |
| `vid` | string | yes | Default VID for the hotkey expression |
| `hotkey` | string | yes | Hotkey expression (same syntax as hotkey rules) |

**Modes:**

| Mode | Behavior |
|------|----------|
| `toggle` | First hotkey fire enables the layer; next fire disables it. The `active` field in config sets the initial state at startup. |
| `while_active` | Layer is active exactly while the hotkey/axis is held. Deactivates on release. |
| `while_not_active` | Layer is active when the hotkey/axis is NOT held. Useful for "default state unless a modifier is held". |

The `hotkey` field uses the existing hotkey expression syntax:
- Single axis: `"ButtonA"`
- Modifier + axis: `"!ButtonBumperLeft+ButtonBack"`
- Multi-device: `"!vid1:ButtonBumperLeft+vid2:ButtonA"`
- Sequence: `"ButtonBack->ButtonA"`

**REST API interaction:**  
REST activate/deactivate (`POST /layers/{id}/activate`, `POST /layers/{id}/deactivate`) continues to work for all layers. Activation triggers and REST control both write the same `active` flag — last write wins. This allows REST to override trigger-driven layers for debugging.

---

## Full Config Example

```json
{
    "layers": [
        {
            "id": "base",
            "name": "Base Layer",
            "active": true,
            "rules": [
                {
                    "type": "simple",
                    "vid": "vid1",
                    "vod": "vgp1",
                    "axes": [
                        { "from": "ButtonA", "to": "Button_1" },
                        { "from": "ButtonB", "to": "Button_2" },
                        { "from": "Stick LX-", "to": "Stick_Left_X-" },
                        { "from": "Stick LX+", "to": "Stick_Left_X+" }
                    ]
                }
            ]
        },
        {
            "id": "stealth",
            "name": "Hide Gamepad 1",
            "active": false,
            "activation": {
                "mode": "toggle",
                "vid": "vid1",
                "hotkey": "!ButtonBumperLeft+ButtonBack"
            },
            "rules": [
                { "type": "vod_state", "vod": "vgp1", "state": "disconnected" }
            ]
        },
        {
            "id": "turbo-fire",
            "name": "Turbo Fire",
            "active": false,
            "activation": {
                "mode": "while_active",
                "vid": "vid1",
                "hotkey": "ButtonBumperRight"
            },
            "rules": [
                {
                    "type": "turbo",
                    "vid": "vid1",
                    "axis": "ButtonA",
                    "on_ms": 80,
                    "off_ms": 40,
                    "initial_delay_ms": 150,
                    "condition": "while_axis_active"
                },
                {
                    "type": "block",
                    "vid": "vid1",
                    "axes": [
                        { "axis": "Stick LX-", "value": 500 },
                        { "axis": "Stick LX+", "value": 500 }
                    ]
                }
            ]
        },
        {
            "id": "auto-fire",
            "name": "Auto Fire (no button hold needed)",
            "active": false,
            "activation": {
                "mode": "while_not_active",
                "vid": "vid1",
                "hotkey": "ButtonBumperLeft"
            },
            "rules": [
                {
                    "type": "turbo",
                    "vid": "vid1",
                    "axis": "ButtonA",
                    "on_ms": 60,
                    "off_ms": 60,
                    "condition": "always"
                }
            ]
        }
    ]
}
```

---

## What Is Not In Scope

- UI for layer management (beyond existing REST API)
- Saving layer active state across restarts (layers always start from `active` field in config)
- Turbo on analog axes with non-binary values (e.g. stick gradually increasing — turbo always snaps between `min_value` and `max_value`)
- Per-layer ordering/priority changes at runtime (order is fixed by config array position)
