# Pico Configuration Sync Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace runtime dynamic device plug/unplug with a boot-time JSON config sync between mainboard and Pico, using CRC32 handshake to ensure both sides have matching configurations.

**Architecture:** Shared `PicoConfig` structs + parser/serializer (used by both sides) form the foundation. The mainboard loads `emulation_boards` from `config.json`, computes CRC32 of each board's canonical config, and pushes configs to Pico on CRC mismatch. The Pico validates and stores configs in flash, rebooting to apply them. A new `EmulatedDeviceManager` provides a flat indexed device list for runtime axis routing.

**Tech Stack:** C++20, jsmn (single-header JSON tokenizer), CMake (both projects), TinyUSB (Pico), corocrpc RPC (shared). Spec: `docs/superpowers/specs/2026-03-16-pico-config-sync-design.md`.

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `shared/jsmn.h` | Third-party single-header JSON tokenizer |
| Create | `shared/crc32.h` | Header-only CRC32 utility |
| Create | `shared/PicoConfig.h` | `PicoDeviceType`, `PicoDeviceConfig`, `PicoConfig` structs + function declarations |
| Create | `shared/PicoConfig.cpp` | `parsePicoConfig` + `serializePicoConfig` implementations |
| Modify | `shared/rpcinterface.h` | Update `P2M_ON_BOOT`, add `M2P_SET_CONFIGURATION`, remove `M2P_SET_MODE`/`M2P_GET_MODE` |
| Create | `mainboard/src/VirtualOutputDevice.h` | `VirtualOutputDevice` struct (mainboard-side emulated device handle) |
| Create | `mainboard/src/EmulatedDeviceManager.h` | `EmulatedDeviceManager` class declaration |
| Create | `mainboard/src/EmulatedDeviceManager.cpp` | `EmulatedDeviceManager` implementation |
| Create | `mainboard/src/MainboardConfig.h` | `BoardEntry` struct + `loadMainboardConfig()` declaration |
| Create | `mainboard/src/MainboardConfig.cpp` | Parse `emulation_boards` from `config.json` using jsmn |
| Modify | `mainboard/src/EmulationBoard.h` | Add `setConfiguration()` RPC method, store `PicoConfig` |
| Modify | `mainboard/src/main.cpp` | Load board configs, instantiate `EmulatedDeviceManager`, update `P2M_ON_BOOT` handler |
| Modify | `mainboard/config.json` | Add `emulation_boards` field |
| Modify | `mainboard/CMakeLists.txt` | Add `EmulatedDeviceManager.cpp`, `MainboardConfig.cpp`, `PicoConfig.cpp` |
| Modify | `Pico/src/mainPico.cpp` | New boot sequence, `M2P_SET_CONFIGURATION` handler, remove mode handlers |
| Modify | `Pico/CMakeLists.txt` | Add `PicoConfig.cpp` |

---

## Chunk 1: Shared Layer

### Task 1: Vendor jsmn.h

**Files:**
- Create: `shared/jsmn.h`

- [ ] **Step 1: Obtain jsmn.h**

  jsmn is a public-domain/MIT single-header JSON tokenizer. Search for "jsmn zserge" to find the official repository and copy the `jsmn.h` file to `shared/jsmn.h`. The key types used throughout this plan:

  ```c
  typedef enum { JSMN_UNDEFINED=0, JSMN_OBJECT=1, JSMN_ARRAY=2,
                 JSMN_STRING=3, JSMN_PRIMITIVE=4 } jsmntype_t;
  typedef struct { jsmntype_t type; int start; int end; int size; } jsmntok_t;
  typedef struct { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;
  void jsmn_init(jsmn_parser*);
  int  jsmn_parse(jsmn_parser*, const char* js, size_t len,
                  jsmntok_t* tokens, unsigned int num_tokens);
  // returns token count, or JSMN_ERROR_NOMEM(-1), JSMN_ERROR_INVAL(-2), JSMN_ERROR_PART(-3)
  ```

  Note: `size` on OBJECT tokens = number of key-value pairs; on ARRAY = number of elements; on STRING/PRIMITIVE = 0.

- [ ] **Step 2: Verify the file is in place**

  ```bash
  ls -la shared/jsmn.h
  ```
  Expected: file exists, non-zero size.

- [ ] **Step 3: Commit**

  ```bash
  git add shared/jsmn.h
  git commit -m "vendor: add jsmn single-header JSON tokenizer"
  ```

---

### Task 2: Add shared/crc32.h

**Files:**
- Create: `shared/crc32.h`

- [ ] **Step 1: Write the header**

  ```cpp
  // shared/crc32.h
  // Table-based CRC32 (no Pico SDK or TinyUSB dependencies).
  // Note: the fallback configCrc32=0 is a defined constant by convention — do NOT
  // use crc32("",0) for the fallback; set configCrc32=0 unconditionally.
  #pragma once
  #include <cstdint>
  #include <cstddef>

  inline uint32_t crc32(const char* data, size_t len) {
      static const uint32_t table[256] = {
          0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
          0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,
          0x09B64C2B,0x7EB17CBF,0xE7B82D08,0x90BF1D9C,0x1DB71064,0x6AB020F2,
          0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
          0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
          0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
          0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
          0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
          0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F928,0x56B3C9BE,
          0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
          0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
          0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
          0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6748FF,0x086D3D2D,
          0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
          0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
          0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
          0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
          0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
          0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
          0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
          0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
          0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
          0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
          0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
          0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
          0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
          0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
          0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
          0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA8670955,
          0x316658EF,0x466B6879,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
      };
      uint32_t crc = 0xFFFFFFFF;
      for (size_t i = 0; i < len; i++)
          crc = table[(crc ^ (uint8_t)data[i]) & 0xFF] ^ (crc >> 8);
      return crc ^ 0xFFFFFFFF;
  }
  ```

### Task 3: Add shared/PicoConfig.h

**Files:**
- Create: `shared/PicoConfig.h`

- [ ] **Step 1: Write the header**

  ```cpp
  // shared/PicoConfig.h
  // Shared config structs for Pico device layout.
  // No Pico SDK or TinyUSB includes — safe to use on mainboard and Pico.
  #pragma once
  #include <string>
  #include <vector>
  #include <cstdint>
  #include "shared.h"   // for DeviceMode (HID_MODE / XINPUT_MODE)

  // Separate from Pico-only DeviceType in AbstractVirtualDevice.h
  // (which uses GAMEPAD/XINPUT_GAMEPAD naming for internal USB dispatch)
  enum class PicoDeviceType {
      KEYBOARD,
      MOUSE,
      HID_GAMEPAD,
      XBOX360_GAMEPAD
  };

  struct PicoDeviceConfig {
      PicoDeviceType type;
      std::string    name;        // USB interface string; defaults assigned per type if empty
      // hidgamepad fields — required in JSON; C++ defaults for programmatic construction only:
      uint8_t        buttons  = 16;
      uint8_t        axesMask = 0;
      bool           hat      = false; // required field in JSON for hidgamepad
  };

  struct PicoConfig {
      DeviceMode                    mode         = HID_MODE;
      uint16_t                      vid          = 0x120A;
      uint16_t                      pid          = 0x0004;
      std::string                   manufacturer = "InputProxy";
      std::string                   product      = "InputProxy Device";
      std::string                   serial       = "000000";
      std::vector<PicoDeviceConfig> devices;
  };

  // Parse canonical JSON string into PicoConfig.
  // Uses jsmn with a 128-token stack array (max ~102 tokens for 8 HID devices).
  // Accepts decimal integers or "0x..." hex strings for vid/pid.
  // Returns false and populates errorMsg on any error.
  bool parsePicoConfig(const char* json, int len, PicoConfig& out, std::string& errorMsg);

  // Serialize PicoConfig to canonical compact JSON.
  // Fixed key order: mode, vid, pid, manufacturer, product, serial, devices.
  // Per-device: type, name, [buttons, axesmask, hat for hidgamepad].
  // vid/pid as decimal integers. hat as true/false.
  // Must be idempotent: serialize(parse(serialize(x))) == serialize(x).
  std::string serializePicoConfig(const PicoConfig& cfg);
  ```

- [ ] **Step 2: Commit**
empty

---

### Task 4: Add shared/PicoConfig.cpp

**Files:**
- Create: `shared/PicoConfig.cpp`

- [ ] **Step 1: Write implementation**

  ```cpp
  // shared/PicoConfig.cpp
  #define JSMN_STATIC
  #include "jsmn.h"
  #include "PicoConfig.h"
  #include <cstring>
  #include <cstdlib>
  #include <string>

  // ── jsmn helpers ─────────────────────────────────────────────────────────

  static bool tok_eq(const char* js, const jsmntok_t* t, const char* s) {
      int len = t->end - t->start;
      return t->type == JSMN_STRING &&
             len == (int)strlen(s) &&
             strncmp(js + t->start, s, len) == 0;
  }

  static std::string tok_str(const char* js, const jsmntok_t* t) {
      return std::string(js + t->start, t->end - t->start);
  }

  // Accepts bare decimal integer or quoted/bare "0x..." hex string
  static uint32_t tok_uint(const char* js, const jsmntok_t* t) {
      std::string s = tok_str(js, t);
      return (uint32_t)strtoul(s.c_str(), nullptr, 0);
  }

  static bool tok_bool(const char* js, const jsmntok_t* t) {
      return (t->end - t->start) >= 4 &&
             strncmp(js + t->start, "true", 4) == 0;
  }

  // ── parsePicoConfig ───────────────────────────────────────────────────────

  bool parsePicoConfig(const char* json, int len, PicoConfig& out, std::string& errorMsg) {
      jsmn_parser parser;
      jsmntok_t   tokens[128];
      jsmn_init(&parser);
      int r = jsmn_parse(&parser, json, (size_t)len, tokens, 128);
      if (r < 0) {
          errorMsg = r == -1 ? "not enough tokens (config too large)" : "invalid JSON";
          return false;
      }
      if (r == 0 || tokens[0].type != JSMN_OBJECT) {
          errorMsg = "root must be a JSON object";
          return false;
      }

      // Track which required fields were seen
      bool hasMode = false;
      int i = 1; // current token index

      while (i < r) {
          if (tokens[i].type != JSMN_STRING) break; // end of root object
          const jsmntok_t* key = &tokens[i];
          const jsmntok_t* val = &tokens[i + 1];

          if (tok_eq(json, key, "mode")) {
              std::string m = tok_str(json, val);
              if      (m == "hid")    out.mode = HID_MODE;
              else if (m == "xinput") out.mode = XINPUT_MODE;
              else { errorMsg = "mode must be 'hid' or 'xinput'"; return false; }
              hasMode = true;
              i += 2;

          } else if (tok_eq(json, key, "vid")) {
              out.vid = (uint16_t)tok_uint(json, val);
              i += 2;
          } else if (tok_eq(json, key, "pid")) {
              out.pid = (uint16_t)tok_uint(json, val);
              i += 2;
          } else if (tok_eq(json, key, "manufacturer")) {
              out.manufacturer = tok_str(json, val);
              i += 2;
          } else if (tok_eq(json, key, "product")) {
              out.product = tok_str(json, val);
              i += 2;
          } else if (tok_eq(json, key, "serial")) {
              out.serial = tok_str(json, val);
              i += 2;

          } else if (tok_eq(json, key, "devices")) {
              if (val->type != JSMN_ARRAY) {
                  errorMsg = "'devices' must be an array";
                  return false;
              }
              int numDevices = val->size;
              i += 2; // skip "devices" key + array token
              for (int d = 0; d < numDevices; d++) {
                  if (tokens[i].type != JSMN_OBJECT) {
                      errorMsg = "each device must be a JSON object";
                      return false;
                  }
                  int numFields = tokens[i].size;
                  i++; // skip device object token
                  PicoDeviceConfig dev;
                  bool hasType = false, hasButtons = false, hasAxesMask = false, hasHat = false;
                  for (int f = 0; f < numFields; f++) {
                      const jsmntok_t* dk = &tokens[i];
                      const jsmntok_t* dv = &tokens[i + 1];
                      if (tok_eq(json, dk, "type")) {
                          std::string t = tok_str(json, dv);
                          if      (t == "keyboard")       dev.type = PicoDeviceType::KEYBOARD;
                          else if (t == "mouse")          dev.type = PicoDeviceType::MOUSE;
                          else if (t == "hidgamepad")     dev.type = PicoDeviceType::HID_GAMEPAD;
                          else if (t == "xbox360gamepad") dev.type = PicoDeviceType::XBOX360_GAMEPAD;
                          else { errorMsg = "unknown device type: " + t; return false; }
                          hasType = true;
                      } else if (tok_eq(json, dk, "name")) {
                          dev.name = tok_str(json, dv);
                      } else if (tok_eq(json, dk, "buttons")) {
                          dev.buttons = (uint8_t)tok_uint(json, dv);
                          hasButtons = true;
                      } else if (tok_eq(json, dk, "axesmask")) {
                          dev.axesMask = (uint8_t)tok_uint(json, dv);
                          hasAxesMask = true;
                      } else if (tok_eq(json, dk, "hat")) {
                          dev.hat = tok_bool(json, dv);
                          hasHat = true;
                      }
                      i += 2;
                  }
                  if (!hasType) { errorMsg = "device missing 'type'"; return false; }
                  if (dev.type == PicoDeviceType::HID_GAMEPAD && (!hasButtons || !hasAxesMask || !hasHat)) {
                      errorMsg = "hidgamepad requires 'buttons', 'axesmask', and 'hat'";
                      return false;
                  }
                  out.devices.push_back(dev);
              }
          } else {
              i += 2; // skip unknown key + value
          }
      }

      if (!hasMode) { errorMsg = "missing required field 'mode'"; return false; }
      if (out.devices.empty()) { errorMsg = "'devices' array is empty or missing"; return false; }

      // Mode consistency check
      for (const auto& d : out.devices) {
          bool isXInput = (d.type == PicoDeviceType::XBOX360_GAMEPAD);
          if (out.mode == XINPUT_MODE && !isXInput) {
              errorMsg = "xinput mode only allows xbox360gamepad devices";
              return false;
          }
          if (out.mode == HID_MODE && isXInput) {
              errorMsg = "hid mode does not allow xbox360gamepad devices";
              return false;
          }
      }

      // Device count limits
      int maxDevices = (out.mode == XINPUT_MODE) ? 4 : 8;
      if ((int)out.devices.size() > maxDevices) {
          errorMsg = "too many devices (max " + std::to_string(maxDevices) +
                     " for " + (out.mode == XINPUT_MODE ? "xinput" : "hid") + " mode)";
          return false;
      }

      return true;
  }

  // ── serializePicoConfig ───────────────────────────────────────────────────

  static std::string escapeJson(const std::string& s) {
      std::string out;
      out.reserve(s.size());
      for (char c : s) {
          switch (c) {
              case '"':  out += "\\\""; break;
              case '\\': out += "\\\\"; break;
              case '\b': out += "\\b";  break;
              case '\f': out += "\\f";  break;
              case '\n': out += "\\n";  break;
              case '\r': out += "\\r";  break;
              case '\t': out += "\\t";  break;
              default:   out += c;      break;
          }
      }
      return out;
  }

  static std::string typeToJsonString(PicoDeviceType t) {
      switch (t) {
          case PicoDeviceType::KEYBOARD:        return "keyboard";
          case PicoDeviceType::MOUSE:           return "mouse";
          case PicoDeviceType::HID_GAMEPAD:     return "hidgamepad";
          case PicoDeviceType::XBOX360_GAMEPAD: return "xbox360gamepad";
      }
      return "keyboard"; // unreachable
  }

  std::string serializePicoConfig(const PicoConfig& cfg) {
      std::string s;
      s += "{\"mode\":\"";
      s += (cfg.mode == XINPUT_MODE) ? "xinput" : "hid";
      s += "\"";
      s += ",\"vid\":"  + std::to_string(cfg.vid);
      s += ",\"pid\":"  + std::to_string(cfg.pid);
      s += ",\"manufacturer\":\"" + escapeJson(cfg.manufacturer) + "\"";
      s += ",\"product\":\""      + escapeJson(cfg.product)      + "\"";
      s += ",\"serial\":\""       + escapeJson(cfg.serial)       + "\"";
      s += ",\"devices\":[";
      for (size_t i = 0; i < cfg.devices.size(); i++) {
          if (i > 0) s += ",";
          const auto& d = cfg.devices[i];
          s += "{\"type\":\"" + typeToJsonString(d.type) + "\"";
          s += ",\"name\":\"" + escapeJson(d.name) + "\"";
          if (d.type == PicoDeviceType::HID_GAMEPAD) {
              s += ",\"buttons\":"  + std::to_string(d.buttons);
              s += ",\"axesmask\":" + std::to_string(d.axesMask);
              s += ",\"hat\":"      + std::string(d.hat ? "true" : "false");
          }
          s += "}";
      }
      s += "]}";
      return s;
  }
  ```

- [ ] **Step 2: Commit**

empty

---

### Task 5: Write and run host-side unit tests for PicoConfig + crc32

**Files:**
- Create: `shared/test_picoconfig.cpp`

- [ ] **Step 1: Write tests**

  ```cpp
  // shared/test_picoconfig.cpp
  // Compile: g++ -std=c++20 -I. shared/PicoConfig.cpp shared/shared.cpp shared/test_picoconfig.cpp -o /tmp/test_pico && /tmp/test_pico
  #include <cassert>
  #include <iostream>
  #include <cstring>
  #include "PicoConfig.h"
  #include "crc32.h"

  static int passed = 0, failed = 0;
  #define CHECK(expr) do { if (expr) { passed++; } else { failed++; std::cerr << "FAIL: " #expr " at line " << __LINE__ << "\n"; } } while(0)

  void test_serialize_hid() {
      PicoConfig cfg;
      cfg.mode = HID_MODE;
      cfg.vid = 0x120A; cfg.pid = 0x0004;
      cfg.manufacturer = "InputProxy"; cfg.product = "Test"; cfg.serial = "001";
      PicoDeviceConfig kbd; kbd.type = PicoDeviceType::KEYBOARD; kbd.name = "KB";
      cfg.devices.push_back(kbd);
      std::string s = serializePicoConfig(cfg);
      CHECK(s == "{\"mode\":\"hid\",\"vid\":4618,\"pid\":4,\"manufacturer\":\"InputProxy\","
                 "\"product\":\"Test\",\"serial\":\"001\",\"devices\":[{\"type\":\"keyboard\",\"name\":\"KB\"}]}");
  }

  void test_serialize_xinput() {
      PicoConfig cfg;
      cfg.mode = XINPUT_MODE;
      cfg.vid = 0x045E; cfg.pid = 0x028E;
      cfg.manufacturer = "Microsoft"; cfg.product = "Xbox 360 Controller"; cfg.serial = "000000";
      PicoDeviceConfig gp; gp.type = PicoDeviceType::XBOX360_GAMEPAD; gp.name = "Pad 1";
      cfg.devices.push_back(gp);
      std::string s = serializePicoConfig(cfg);
      CHECK(s == "{\"mode\":\"xinput\",\"vid\":1118,\"pid\":654,\"manufacturer\":\"Microsoft\","
                 "\"product\":\"Xbox 360 Controller\",\"serial\":\"000000\","
                 "\"devices\":[{\"type\":\"xbox360gamepad\",\"name\":\"Pad 1\"}]}");
  }

  void test_serialize_hidgamepad() {
      PicoConfig cfg;
      cfg.mode = HID_MODE;
      cfg.vid = 0x120A; cfg.pid = 4; cfg.manufacturer = "X"; cfg.product = "Y"; cfg.serial = "Z";
      PicoDeviceConfig gp;
      gp.type = PicoDeviceType::HID_GAMEPAD; gp.name = "Pad";
      gp.buttons = 8; gp.axesMask = 3; gp.hat = true;
      cfg.devices.push_back(gp);
      std::string s = serializePicoConfig(cfg);
      CHECK(s.find("\"buttons\":8") != std::string::npos);
      CHECK(s.find("\"axesmask\":3") != std::string::npos);
      CHECK(s.find("\"hat\":true") != std::string::npos);
  }

  void test_parse_roundtrip() {
      PicoConfig cfg;
      cfg.mode = HID_MODE; cfg.vid = 4618; cfg.pid = 4;
      cfg.manufacturer = "A"; cfg.product = "B"; cfg.serial = "C";
      PicoDeviceConfig d; d.type = PicoDeviceType::MOUSE; d.name = "M";
      cfg.devices.push_back(d);
      std::string json = serializePicoConfig(cfg);
      PicoConfig cfg2;
      std::string err;
      CHECK(parsePicoConfig(json.c_str(), (int)json.size(), cfg2, err));
      CHECK(cfg2.mode == HID_MODE);
      CHECK(cfg2.vid == 4618);
      CHECK(cfg2.devices.size() == 1);
      CHECK(cfg2.devices[0].type == PicoDeviceType::MOUSE);
      // Idempotency: serialize again and check same output
      CHECK(serializePicoConfig(cfg2) == json);
  }

  void test_parse_with_hidgamepad() {
      const char* json = "{\"mode\":\"hid\",\"vid\":4618,\"pid\":4,\"manufacturer\":\"X\","
                         "\"product\":\"Y\",\"serial\":\"Z\","
                         "\"devices\":[{\"type\":\"hidgamepad\",\"name\":\"P\","
                         "\"buttons\":8,\"axesmask\":3,\"hat\":true}]}";
      PicoConfig cfg;
      std::string err;
      CHECK(parsePicoConfig(json, (int)strlen(json), cfg, err));
      CHECK(cfg.devices[0].type == PicoDeviceType::HID_GAMEPAD);
      CHECK(cfg.devices[0].buttons == 8);
      CHECK(cfg.devices[0].axesMask == 3);
      CHECK(cfg.devices[0].hat == true);
  }

  void test_parse_hex_vid() {
      const char* json = "{\"mode\":\"hid\",\"vid\":\"0x120A\",\"pid\":\"0x0004\","
                         "\"manufacturer\":\"X\",\"product\":\"Y\",\"serial\":\"Z\","
                         "\"devices\":[{\"type\":\"keyboard\",\"name\":\"K\"}]}";
      PicoConfig cfg;
      std::string err;
      CHECK(parsePicoConfig(json, (int)strlen(json), cfg, err));
      CHECK(cfg.vid == 0x120A);
      CHECK(cfg.pid == 0x0004);
  }

  void test_validation_mode_mismatch() {
      const char* json = "{\"mode\":\"xinput\",\"vid\":1118,\"pid\":654,"
                         "\"manufacturer\":\"M\",\"product\":\"P\",\"serial\":\"S\","
                         "\"devices\":[{\"type\":\"keyboard\",\"name\":\"K\"}]}";
      PicoConfig cfg;
      std::string err;
      CHECK(!parsePicoConfig(json, (int)strlen(json), cfg, err));
      CHECK(!err.empty());
  }

  void test_validation_hidgamepad_missing_fields() {
      const char* json = "{\"mode\":\"hid\",\"vid\":4618,\"pid\":4,"
                         "\"manufacturer\":\"X\",\"product\":\"Y\",\"serial\":\"Z\","
                         "\"devices\":[{\"type\":\"hidgamepad\",\"name\":\"P\","
                         "\"buttons\":8}]}"; // missing axesmask and hat
      PicoConfig cfg;
      std::string err;
      CHECK(!parsePicoConfig(json, (int)strlen(json), cfg, err));
  }

  void test_crc32_stable() {
      const char* s = "hello";
      uint32_t c1 = crc32(s, 5);
      uint32_t c2 = crc32(s, 5);
      CHECK(c1 == c2);
      CHECK(c1 == 0x3610A686u); // known CRC32 of "hello"
  }

  void test_crc32_different_inputs() {
      const char* a = "abc";
      const char* b = "abd";
      CHECK(crc32(a, 3) != crc32(b, 3));
  }

  int main() {
      test_serialize_hid();
      test_serialize_xinput();
      test_serialize_hidgamepad();
      test_parse_roundtrip();
      test_parse_with_hidgamepad();
      test_parse_hex_vid();
      test_validation_mode_mismatch();
      test_validation_hidgamepad_missing_fields();
      test_crc32_stable();
      test_crc32_different_inputs();
      std::cout << passed << " passed, " << failed << " failed\n";
      return failed ? 1 : 0;
  }
  ```

- [ ] **Step 2: Compile and run**

  ```bash
  cd /Users/dsavchenko/Documents/work/projects/cpp/InputProxy
  g++ -std=c++20 -I. -Ishared shared/PicoConfig.cpp shared/shared.cpp shared/test_picoconfig.cpp -o /tmp/test_pico && /tmp/test_pico
  ```
  Expected output: `10 passed, 0 failed`

  If `test_crc32_stable` fails, verify `crc32("hello", 5) == 0x3610A686`. If different, the table is wrong — cross-check against a known-good CRC32 table or tool (`echo -n "hello" | crc32`).

- [ ] **Step 3: Commit**

empty

---

### Task 6: Update shared/rpcinterface.h

**Files:**
- Modify: `shared/rpcinterface.h`

- [ ] **Step 1: Read current file**

  Read `shared/rpcinterface.h` and confirm current contents.

- [ ] **Step 2: Replace with updated version**

  ```cpp
  // shared/rpcinterface.h — Shared RPC interface between Mainboard (RPi4) and Pico
  // Used with corocrpc: rpc.call(METHOD_ID, arg) / rpc.registerMethod(METHOD_ID, handler).
  // Argument layout documented per method as: args: <type> <name>, ... | returns: <type>
  #pragma once
  #include <cstdint>

  // ── Pico2Main method IDs ─────────────────────────────────────────────────
  // Methods that Pico calls on Main (Main registers the handlers as server).

  enum Pico2MainMethod : uint16_t {
      P2M_PING        = 1, /* args: int32 val                                   | returns: int32 val */
      P2M_DEBUG_PRINT = 2, /* args: string message                              | returns: void */
      P2M_ON_BOOT     = 3, /* args: string picoId, uint32 configCrc32           | returns: bool success */
  };

  // ── Main2Pico method IDs ─────────────────────────────────────────────────
  // Methods that Main calls on Pico (Pico registers the handlers as server).

  enum Main2PicoMethod : uint16_t {
      M2P_PING              = 1, /* args: int32 val                             | returns: int32 val */
      M2P_SET_LED           = 2, /* args: bool state                            | returns: void */
      M2P_GET_LED_STATUS    = 3, /* args: void                                  | returns: bool state */
      M2P_REBOOT_FLASH_MODE = 4, /* args: void                                  | returns: bool */
      M2P_REBOOT            = 5, /* args: void                                  | returns: void (Pico reboots) */
      M2P_SET_AXIS          = 6, /* args: int32 device, int32 axis, int32 value | returns: void */
      // 7 = M2P_SET_MODE — REMOVED (mode is now config-driven)
      // 8 = M2P_GET_MODE — REMOVED (mode is now config-driven)
      M2P_SET_CONFIGURATION = 9, /* args: string configJson
                                    returns: bool ok, string errorMsg
                                    Encoding: putBool(ok), putString(errorMsg)
                                    On success: Pico replies {true,""} then reboots.
                                    On failure: Pico replies {false, reason}, no reboot. */
  };
  ```

- [ ] **Step 3: Commit**

empty
---

## Chunk 2: Mainboard

### Task 7: Add mainboard/src/VirtualOutputDevice.h

**Files:**
- Create: `mainboard/src/VirtualOutputDevice.h`

- [ ] **Step 1: Write the header**

  ```cpp
  // mainboard/src/VirtualOutputDevice.h
  // Mainboard-side handle for a single Pico-emulated device.
  // Uses PicoDeviceType from shared/PicoConfig.h, NOT the Pico-internal DeviceType.
  #pragma once
  #include <string>
  #include "PicoConfig.h"  // ../shared/ is in include_directories
  #include "shared.h"      // for AxisTable

  // Forward declaration
  class EmulationBoard;

  struct VirtualOutputDevice {
      std::string     id;          // human-readable id from mainboard config ("vkbd1", etc.)
      int             slotIndex;   // 0-based position in Pico's device list (used in M2P_SET_AXIS)
      PicoDeviceType  type;
      AxisTable       axisTable;   // built from forKeyboard()/forMouse()/forHidGamepad()/forXbox360()
      EmulationBoard* board;       // back-pointer to owning Pico board; nullptr until activated

      // Dispatch axis value to the Pico via RPC. No-op if board is nullptr or inactive.
      void setAxis(int axis, int value);
  };
  ```

- [ ] **Step 2: Commit**

  ```bash
  git add mainboard/src/VirtualOutputDevice.h
  git commit -m "feat: add VirtualOutputDevice header"
  ```

---

### Task 8: Add EmulatedDeviceManager

**Files:**
- Create: `mainboard/src/EmulatedDeviceManager.h`
- Create: `mainboard/src/EmulatedDeviceManager.cpp`

- [ ] **Step 1: Write the header**

  ```cpp
  // mainboard/src/EmulatedDeviceManager.h
  #pragma once
  #include <vector>
  #include <map>
  #include <string>
  #include "VirtualOutputDevice.h"

  class EmulationBoard;

  class EmulatedDeviceManager {
  public:
      // Called when onBoot CRC matches.
      // First call for a board: appends its devices and builds idToIndex entries.
      // Subsequent calls (re-boot): replaces board pointer in-place at same indices.
      void registerBoard(EmulationBoard* board,
                         const std::vector<VirtualOutputDevice>& devices);

      // Sets board->active = false. Leaves devices in vector.
      // setAxis calls on these devices are silently dropped until re-activation.
      // Trigger: RPC timeout, UART disconnect (future implementation).
      void deactivateBoard(EmulationBoard* board);

      // Runtime axis dispatch. Silently drops if device out of range or board inactive.
      void setAxis(int deviceIndex, int axis, int value);

      // Config-time string-id to flat-index resolution. Returns -1 if not found.
      int resolveId(const std::string& id) const;

      // For inspection (e.g., REST API)
      const std::vector<VirtualOutputDevice>& getDevices() const { return devices; }

  private:
      std::vector<VirtualOutputDevice> devices;
      std::map<std::string, int>       idToIndex;
  };
  ```

- [ ] **Step 2: Write the implementation**

  ```cpp
  // mainboard/src/EmulatedDeviceManager.cpp
  #include "EmulatedDeviceManager.h"
  #include "EmulationBoard.h"
  #include <iostream>

  void EmulatedDeviceManager::registerBoard(EmulationBoard* board,
                                             const std::vector<VirtualOutputDevice>& newDevices) {
      // Detect re-boot: on re-boot the same EmulationBoard object is found in
      // emulationBoards by serialString and reused — the same pointer is passed again.
      // If any existing device already points to this board, it's a re-boot: no-op.
      // (board->active is set to true by the caller before registerBoard is called.)
      for (const auto& d : devices) {
          if (d.board == board) return;
      }

      // First registration: append all devices and build idToIndex entries.
      for (auto d : newDevices) {
          d.board = board;
          int idx = (int)devices.size();
          idToIndex[d.id] = idx;
          devices.push_back(std::move(d));
      }
  }

  void EmulatedDeviceManager::deactivateBoard(EmulationBoard* board) {
      if (board) board->active = false;
  }

  void EmulatedDeviceManager::setAxis(int deviceIndex, int axis, int value) {
      if (deviceIndex < 0 || deviceIndex >= (int)devices.size()) return;
      auto& d = devices[deviceIndex];
      if (d.board == nullptr || !d.board->active) return;
      d.setAxis(axis, value);
  }

  int EmulatedDeviceManager::resolveId(const std::string& id) const {
      auto it = idToIndex.find(id);
      return (it != idToIndex.end()) ? it->second : -1;
  }
  ```

- [ ] **Step 3: Write the VirtualOutputDevice::setAxis implementation**

  Add to `mainboard/src/VirtualOutputDevice.h` (inline, or create `VirtualOutputDevice.cpp`). Since it needs `EmulationBoard`, add it inline with a forward-declared `EmulationBoard::setAxis`:

  In `VirtualOutputDevice.h`, after the struct definition add:

  ```cpp
  // Inline implementation — requires EmulationBoard to be fully defined.
  // Include EmulationBoard.h after this header wherever setAxis() is called.
  ```

  Create `mainboard/src/VirtualOutputDevice.cpp`:
  ```cpp
  // mainboard/src/VirtualOutputDevice.cpp
  #include "VirtualOutputDevice.h"
  #include "EmulationBoard.h"

  void VirtualOutputDevice::setAxis(int axis, int value) {
      if (board == nullptr || !board->active) return;
      board->setAxis(slotIndex, axis, value);
  }
  ```

- [ ] **Step 4: Commit**

  ```bash
  git add mainboard/src/EmulatedDeviceManager.h mainboard/src/EmulatedDeviceManager.cpp mainboard/src/VirtualOutputDevice.cpp
  git commit -m "feat: add EmulatedDeviceManager and VirtualOutputDevice implementation"
  ```

---

### Task 9: Add MainboardConfig

**Files:**
- Create: `mainboard/src/MainboardConfig.h`
- Create: `mainboard/src/MainboardConfig.cpp`

- [ ] **Step 1: Write the header**

  ```cpp
  // mainboard/src/MainboardConfig.h
  // Loads emulation_boards from the mainboard config.json using jsmn.
  #pragma once
  #include <string>
  #include <vector>
  #include "VirtualOutputDevice.h"
  #include "PicoConfig.h"  // ../shared/ is in include_directories

  struct BoardEntry {
      std::string picoId;
      PicoConfig  config;
      std::vector<std::string> deviceIds; // human-readable IDs in same order as config.devices
  };

  // Read config.json and return all emulation_boards entries.
  // Returns empty vector on file-not-found or parse failure (logs to stderr).
  std::vector<BoardEntry> loadMainboardConfig(const std::string& path);

  // Build a VirtualOutputDevice list for a board entry.
  // Called after loadMainboardConfig when registering into EmulatedDeviceManager.
  std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry);
  ```

- [ ] **Step 2: Write the implementation**

  ```cpp
  // mainboard/src/MainboardConfig.cpp
  #define JSMN_STATIC
  #include "jsmn.h"
  #include "MainboardConfig.h"
  #include "shared.h"  // ../shared/ is in include_directories
  #include <fstream>
  #include <sstream>
  #include <iostream>
  #include <cstring>
  #include <cstdlib>

  // Returns the number of jsmn tokens in the subtree rooted at tokens[i]
  // (inclusive of the root token itself).
  //
  // jsmn ARRAY.size  = number of elements (each element = 1 node).
  // jsmn OBJECT.size = number of key-value pairs (each pair = 2 nodes: key + value).
  // We must multiply children by 2 for objects so every child token is visited.
  static int countSubtree(const jsmntok_t* tokens, int i) {
      int total = 1;
      int children = tokens[i].size;
      if (tokens[i].type == JSMN_OBJECT) children *= 2; // pairs → individual tokens
      i++;
      for (int c = 0; c < children; c++) {
          int sub = countSubtree(tokens, i);
          i += sub;
          total += sub;
      }
      return total;
  }

  static bool tok_eq(const char* js, const jsmntok_t* t, const char* s) {
      int len = t->end - t->start;
      return t->type == JSMN_STRING && len == (int)strlen(s) &&
             strncmp(js + t->start, s, len) == 0;
  }
  static std::string tok_str(const char* js, const jsmntok_t* t) {
      return std::string(js + t->start, t->end - t->start);
  }
  static uint32_t tok_uint(const char* js, const jsmntok_t* t) {
      return (uint32_t)strtoul(tok_str(js, t).c_str(), nullptr, 0);
  }
  static bool tok_bool(const char* js, const jsmntok_t* t) {
      return (t->end - t->start) >= 4 && strncmp(js + t->start, "true", 4) == 0;
  }

  std::vector<BoardEntry> loadMainboardConfig(const std::string& path) {
      std::ifstream f(path);
      if (!f) { std::cerr << "[config] cannot open " << path << "\n"; return {}; }
      std::ostringstream ss; ss << f.rdbuf();
      std::string json = ss.str();

      // Use a larger token pool since config.json has more fields
      std::vector<jsmntok_t> tokens(512);
      jsmn_parser parser;
      jsmn_init(&parser);
      int r = jsmn_parse(&parser, json.c_str(), json.size(), tokens.data(), (unsigned)tokens.size());
      if (r < 0 || tokens[0].type != JSMN_OBJECT) {
          std::cerr << "[config] failed to parse " << path << "\n"; return {};
      }

      std::vector<BoardEntry> result;

      // Find "emulation_boards" array in root object
      int i = 1;
      while (i < r) {
          if (tokens[i].type != JSMN_STRING) break;
          const jsmntok_t* key = &tokens[i];
          const jsmntok_t* val = &tokens[i + 1];

          if (tok_eq(json.c_str(), key, "emulation_boards")) {
              if (val->type != JSMN_ARRAY) break;
              int numBoards = val->size;
              i += 2; // skip key + array token
              for (int b = 0; b < numBoards; b++) {
                  if (tokens[i].type != JSMN_OBJECT) break;
                  int numFields = tokens[i].size;
                  i++; // skip board object token
                  BoardEntry entry;
                  entry.config.vid = 0x120A; entry.config.pid = 0x0004;
                  entry.config.manufacturer = "InputProxy";
                  entry.config.product = "InputProxy Device";
                  entry.config.serial = "000000";

                  for (int f = 0; f < numFields; f++) {
                      const jsmntok_t* bk = &tokens[i];
                      const jsmntok_t* bv = &tokens[i + 1];
                      if      (tok_eq(json.c_str(), bk, "id"))           { entry.picoId = tok_str(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "vid"))          { entry.config.vid = (uint16_t)tok_uint(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "pid"))          { entry.config.pid = (uint16_t)tok_uint(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "manufacturer")) { entry.config.manufacturer = tok_str(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "product"))      { entry.config.product = tok_str(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "serial"))       { entry.config.serial = tok_str(json.c_str(), bv); i += 2; }
                      else if (tok_eq(json.c_str(), bk, "devices")) {
                          if (bv->type != JSMN_ARRAY) { i += 2; continue; }
                          int numDev = bv->size;
                          i += 2; // skip "devices" key + array token
                          for (int d = 0; d < numDev; d++) {
                              if (tokens[i].type != JSMN_OBJECT) break;
                              int nf = tokens[i].size;
                              i++;
                              std::string devId;
                              PicoDeviceConfig dev;
                              bool hasType = false, hasButtons = false, hasAxesMask = false, hasHat = false;
                              for (int ff = 0; ff < nf; ff++) {
                                  const jsmntok_t* dk = &tokens[i];
                                  const jsmntok_t* dv = &tokens[i + 1];
                                  if      (tok_eq(json.c_str(), dk, "id"))       { devId = tok_str(json.c_str(), dv); }
                                  else if (tok_eq(json.c_str(), dk, "type")) {
                                      std::string t = tok_str(json.c_str(), dv);
                                      if      (t == "keyboard")       dev.type = PicoDeviceType::KEYBOARD;
                                      else if (t == "mouse")          dev.type = PicoDeviceType::MOUSE;
                                      else if (t == "hidgamepad")     dev.type = PicoDeviceType::HID_GAMEPAD;
                                      else if (t == "xbox360gamepad") dev.type = PicoDeviceType::XBOX360_GAMEPAD;
                                      hasType = true;
                                  }
                                  else if (tok_eq(json.c_str(), dk, "name"))     { dev.name = tok_str(json.c_str(), dv); }
                                  else if (tok_eq(json.c_str(), dk, "buttons"))  { dev.buttons = (uint8_t)tok_uint(json.c_str(), dv); hasButtons = true; }
                                  else if (tok_eq(json.c_str(), dk, "axesmask")) { dev.axesMask = (uint8_t)tok_uint(json.c_str(), dv); hasAxesMask = true; }
                                  else if (tok_eq(json.c_str(), dk, "hat"))      { dev.hat = tok_bool(json.c_str(), dv); hasHat = true; }
                                  i += 2;
                              }
                              if (hasType) {
                                  entry.config.devices.push_back(dev);
                                  entry.deviceIds.push_back(devId.empty() ? "dev_" + std::to_string(d) : devId);
                              }
                          }
                      } else {
                          i += 2;
                      }
                  }
                  // Infer mode from device types
                  bool hasXInput = false;
                  for (const auto& d : entry.config.devices)
                      if (d.type == PicoDeviceType::XBOX360_GAMEPAD) hasXInput = true;
                  entry.config.mode = hasXInput ? XINPUT_MODE : HID_MODE;

                  if (!entry.picoId.empty() && !entry.config.devices.empty())
                      result.push_back(std::move(entry));
              }
          } else {
              // Skip unknown root key + its value. Value may be array/object, so use
              // countSubtree to skip all child tokens correctly.
              // i is at the key STRING token; i+1 is the value (any type).
              i += 1 + countSubtree(tokens.data(), i + 1);
          }
      }
      return result;
  }

  std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry) {
      std::vector<VirtualOutputDevice> result;
      for (size_t i = 0; i < entry.config.devices.size(); i++) {
          const auto& d = entry.config.devices[i];
          VirtualOutputDevice vd;
          vd.id         = (i < entry.deviceIds.size()) ? entry.deviceIds[i] : "dev_" + std::to_string(i);
          vd.slotIndex  = (int)i;
          vd.type       = d.type;
          vd.board      = nullptr; // filled in by EmulatedDeviceManager::registerBoard
          switch (d.type) {
              case PicoDeviceType::KEYBOARD:        vd.axisTable = AxisTable::forKeyboard();   break;
              case PicoDeviceType::MOUSE:           vd.axisTable = AxisTable::forMouse();      break;
              case PicoDeviceType::HID_GAMEPAD:     vd.axisTable = AxisTable::forHidGamepad(); break;
              case PicoDeviceType::XBOX360_GAMEPAD: vd.axisTable = AxisTable::forXbox360();    break;
          }
          result.push_back(std::move(vd));
      }
      return result;
  }
  ```

  **Note on root-level skip:** `countSubtree` handles arrays and objects of any depth, so `emulation_boards` can appear anywhere in `config.json`. However, placing it first (Task 12) minimises token consumption by the parser and is a good habit.

- [ ] **Step 3: Commit**

  ```bash
  git add mainboard/src/MainboardConfig.h mainboard/src/MainboardConfig.cpp
  git commit -m "feat: add MainboardConfig loader (emulation_boards parser)"
  ```

---

### Task 10: Update mainboard/src/EmulationBoard.h

**Files:**
- Modify: `mainboard/src/EmulationBoard.h`

- [ ] **Step 1: Read the current file**

  Read `mainboard/src/EmulationBoard.h` to see current state.

- [ ] **Step 2: Add setConfiguration method and picoConfig member**

  Add `#include "PicoConfig.h"` and `#include "crc32.h"` to includes (`../shared/` is in `include_directories`).

  Add to the `EmulationBoard` class:
  ```cpp
  PicoConfig picoConfig;  // the config this board should be running

  // Sends M2P_SET_CONFIGURATION. Returns true if Pico accepted (will reboot).
  // Returns false with errorMsg populated if Pico rejected.
  bool setConfiguration(const std::string& configJson, std::string& errorMsg) {
      corocrpc::RpcArg* arg = rpc->getRpcArg();
      arg->putString(configJson.c_str());
      corocrpc::RpcResult result = rpc->call(M2P_SET_CONFIGURATION, arg);
      bool ok = false;
      errorMsg = "timeout";
      if (result.arg != nullptr) {
          ok = result.arg->getBool();
          char err[256] = {};
          result.arg->getString(err, sizeof(err));
          errorMsg = err;
      }
      rpc->disposeRpcResult(result);
      rpc->disposeRpcArg(arg);
      return ok;
  }
  ```

  Remove `setMode()` and `getMode()` methods (they're no longer in the RPC interface).

- [ ] **Step 3: Commit**

  ```bash
  git add mainboard/src/EmulationBoard.h
  git commit -m "feat: update EmulationBoard — add setConfiguration, remove setMode/getMode"
  ```

---

### Task 11: Update mainboard/src/main.cpp

**Files:**
- Modify: `mainboard/src/main.cpp`
- Modify: `mainboard/src/RestApi.h`
- Modify: `mainboard/src/RestApi.cpp`

- [ ] **Step 1: Read the current file**

  Read `mainboard/src/main.cpp` in full.

- [ ] **Step 2: Add includes and global EmulatedDeviceManager**

  Add at the top of main.cpp:
  ```cpp
  #include "EmulatedDeviceManager.h"
  #include "MainboardConfig.h"
  #include "PicoConfig.h"   // ../shared/ is in include_directories
  #include "crc32.h"
  ```

  After the `emulationBoards` declaration, add:
  ```cpp
  EmulatedDeviceManager* emulatedDeviceManager = nullptr;
  std::vector<BoardEntry> boardConfigs;          // loaded from config.json at startup
  ```

- [ ] **Step 3: Replace the P2M_ON_BOOT handler**

  Find the existing `P2M_ON_BOOT` handler lambda in `initRpcSystem()` and replace it:

  ```cpp
  // onBoot(string picoId, uint32 configCrc32) → bool success
  rpc->registerMethod(P2M_ON_BOOT, [&link, rpc](RpcArg* arg) -> RpcArg* {
      if (!emulatedDeviceManager) {
          std::cerr << "[UART" << link.channel << "] onBoot called before EmulatedDeviceManager init\n";
          RpcArg* out = rpc->getRpcArg(); out->putBool(false); return out;
      }
      char picoIdBuf[64] = {};
      arg->getString(picoIdBuf, sizeof(picoIdBuf));
      // corocrpc encodes all integers as int32. Read as int32, then reinterpret bits
      // as uint32. Values with high bit set arrive as negative int32 but recover
      // correctly: e.g. 0xFFFFFFFF → -1 → (uint32_t)-1 == 0xFFFFFFFF.
      uint32_t receivedCrc = static_cast<uint32_t>(arg->getInt32());
      std::string picoId(picoIdBuf);

      std::cout << "[UART" << link.channel << "] onBoot picoId=" << picoId
                << " crc=0x" << std::hex << receivedCrc << std::dec << std::endl;

      // Find this board in loaded configs
      const BoardEntry* entry = nullptr;
      for (const auto& e : boardConfigs)
          if (e.picoId == picoId) { entry = &e; break; }

      if (!entry) {
          std::cerr << "[UART" << link.channel << "] WARNING: unknown picoId=" << picoId
                    << " — add to emulation_boards in config.json" << std::endl;
          RpcArg* out = rpc->getRpcArg();
          out->putBool(false);
          return out;
      }

      // Compute CRC of our canonical config
      std::string canonical = serializePicoConfig(entry->config);
      uint32_t expectedCrc  = crc32(canonical.c_str(), canonical.size());

      // Find or create EmulationBoard
      EmulationBoard* board = nullptr;
      for (auto& b : emulationBoards)
          if (b.serialString == picoId) { board = &b; break; }
      if (!board) {
          EmulationBoard newBoard;
          newBoard.id           = nextEmulationBoardId++;
          newBoard.serialString = picoId;
          newBoard.rpc          = rpc;
          newBoard.uartChannel  = link.channel;
          newBoard.active       = false;
          newBoard.picoConfig   = entry->config;
          emulationBoards.push_back(std::move(newBoard));
          board = &emulationBoards.back();
      } else {
          board->rpc         = rpc;
          board->uartChannel = link.channel;
          board->picoConfig  = entry->config;
      }

      if (receivedCrc == expectedCrc) {
          board->active = true;
          auto vdevices = buildVirtualDevices(*entry);
          emulatedDeviceManager->registerBoard(board, vdevices);
          std::cout << "[UART" << link.channel << "] picoId=" << picoId
                    << " config match — board active" << std::endl;
          RpcArg* out = rpc->getRpcArg();
          out->putBool(true);
          return out;
      }

      // CRC mismatch — push config
      std::cout << "[UART" << link.channel << "] picoId=" << picoId
                << " CRC mismatch (got 0x" << std::hex << receivedCrc
                << " expected 0x" << expectedCrc << std::dec
                << ") — sending setConfiguration" << std::endl;
      std::string errMsg;
      bool ok = board->setConfiguration(canonical, errMsg);
      if (!ok) {
          std::cerr << "[UART" << link.channel << "] setConfiguration rejected: " << errMsg << std::endl;
      }
      RpcArg* out = rpc->getRpcArg();
      out->putBool(false);
      return out;
  });
  ```

- [ ] **Step 4: Load board configs at startup, before initRpcSystem**

  In `_main()`, before `initRpcSystem()`:
  ```cpp
  boardConfigs = loadMainboardConfig("config.json");
  std::cout << "Loaded " << boardConfigs.size() << " emulation board config(s)" << std::endl;
  emulatedDeviceManager = new EmulatedDeviceManager();
  // Reserve capacity so push_back never reallocates — EmulationBoard* pointers stored
  // in VirtualOutputDevice::board must remain stable for the process lifetime.
  emulationBoards.reserve(16);
  ```

- [ ] **Step 5: Pass emulatedDeviceManager to RestApi**

  In `mainboard/src/RestApi.h`, add the parameter to the declaration:
  ```cpp
  // Replace existing signature:
  // void startRestApi(int port, RealDeviceManager* deviceManager, std::vector<EmulationBoard>* boards);
  // With:
  #include "EmulatedDeviceManager.h"

  void startRestApi(int port, RealDeviceManager* deviceManager,
                    std::vector<EmulationBoard>* boards,
                    EmulatedDeviceManager* emulatedDeviceManager);
  ```

  In `mainboard/src/RestApi.cpp`, update the function signature (first line only):
  ```cpp
  // Replace:
  // void startRestApi(int port, RealDeviceManager* deviceManager, std::vector<EmulationBoard>* boards) {
  // With:
  void startRestApi(int port, RealDeviceManager* deviceManager,
                    std::vector<EmulationBoard>* boards,
                    EmulatedDeviceManager* emulatedDeviceManager) {
  ```
  Store it as a local variable for future use:
  ```cpp
      (void)emulatedDeviceManager;  // reserved for future REST endpoints
  ```

  In `mainboard/src/main.cpp`, update the call at the bottom of `_main()`:
  ```cpp
  startRestApi(8080, deviceManager, &emulationBoards, emulatedDeviceManager);
  ```

- [ ] **Step 6: Commit**

  ```bash
  git add mainboard/src/main.cpp mainboard/src/RestApi.h mainboard/src/RestApi.cpp
  git commit -m "feat: wire EmulatedDeviceManager and new P2M_ON_BOOT handler into mainboard"
  ```

---

### Task 12: Update mainboard/config.json

**Files:**
- Modify: `mainboard/config.json`

- [ ] **Step 1: Update the file**

  Put `emulation_boards` first (so the simple skip-parser in MainboardConfig.cpp doesn't have to handle arrays before finding it):

  ```json
  {
      "emulation_boards": [
          {
              "id": "7I2GP",
              "vid": "0x120A",
              "pid": "0x0004",
              "manufacturer": "InputProxy",
              "product": "InputProxy Composite",
              "serial": "20260101",
              "devices": [
                  { "id": "vkbd1",  "type": "keyboard",  "name": "InputProxy Keyboard" },
                  { "id": "vmouse", "type": "mouse",      "name": "InputProxy Mouse" },
                  { "id": "vgp1",   "type": "hidgamepad", "name": "InputProxy Gamepad 1",
                    "buttons": 16, "axesmask": 3, "hat": true },
                  { "id": "vgp2",   "type": "hidgamepad", "name": "InputProxy Gamepad 2",
                    "buttons": 17, "axesmask": 3, "hat": true },
                  { "id": "vgp3",   "type": "hidgamepad", "name": "InputProxy Gamepad 3",
                    "buttons": 18, "axesmask": 3, "hat": true },
                  { "id": "vgp4",   "type": "hidgamepad", "name": "InputProxy Gamepad 4",
                    "buttons": 19, "axesmask": 3, "hat": true }
              ]
          }
      ],
      "emulation_host": [
          { "id": "7I2GP", "mode": "HID_MODE" }
      ],
      "emulated_devices": [],
      "logical_devices": [],
      "real_devices": [],
      "real_to_logical_mapping": [],
      "profiles": [{ "name": "default", "enabled": true }],
      "mapping": []
  }
  ```

  Replace `"id": "7I2GP"` with the actual picoId of your Pico (the 5-character code stored in its flash under `"deviceId"`). You can read it from `PersistentStorage` by observing the `[UART0 LOG] Hello world N` output in the existing firmware, or by looking at the `onBoot` log line from the current firmware.

- [ ] **Step 2: Commit**

  ```bash
  git add mainboard/config.json
  git commit -m "config: update to emulation_boards format"
  ```

---

### Task 13: Update mainboard/CMakeLists.txt

**Files:**
- Modify: `mainboard/CMakeLists.txt`

- [ ] **Step 1: Add new source files**

  The current `add_executable` line is:
  ```cmake
  add_executable(app ../shared/corocgo/corocgo.cpp ../shared/corocgo/corocrpc/corocrpc.cpp ../shared/shared.cpp src/main.cpp src/RealDeviceManager.cpp src/CoHttpServer.cpp src/RestApi.cpp)
  ```

  Replace with:
  ```cmake
  add_executable(app
      ../shared/corocgo/corocgo.cpp
      ../shared/corocgo/corocrpc/corocrpc.cpp
      ../shared/shared.cpp
      ../shared/PicoConfig.cpp
      src/main.cpp
      src/RealDeviceManager.cpp
      src/CoHttpServer.cpp
      src/RestApi.cpp
      src/EmulatedDeviceManager.cpp
      src/VirtualOutputDevice.cpp
      src/MainboardConfig.cpp
  )
  ```

- [ ] **Step 2: Build and verify no errors**

  ```bash
  cd mainboard
  mkdir -p build && cd build
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../aarch64-toolchain.cmake 2>&1 | tail -5
  cmake --build . 2>&1 | tail -20
  ```
  Expected: no compile errors. (Link errors about missing pico libs are expected if building on non-aarch64 host; on RPi4 target the build should succeed.)

- [ ] **Step 3: Commit**

  ```bash
  git add mainboard/CMakeLists.txt
  git commit -m "build: add EmulatedDeviceManager, VirtualOutputDevice, MainboardConfig, PicoConfig to mainboard"
  ```

---

## Chunk 3: Pico

### Task 14: Update Pico/src/mainPico.cpp

**Files:**
- Modify: `Pico/src/mainPico.cpp`

- [ ] **Step 1: Read the current file**

  Read `Pico/src/mainPico.cpp` in full.

- [ ] **Step 2: Add includes**

  Add near the top, after existing includes:
  ```cpp
  #include "../shared/PicoConfig.h"
  #include "../shared/crc32.h"
  ```

- [ ] **Step 3: Replace the boot-time device manager initialization**

  Replace the block that reads `savedMode` and constructs `HidDeviceManager`/`XinputDeviceManager` with config-driven initialization:

  ```cpp
  // -- Config-driven initialization --
  std::string configJson    = persistentStorage.get("config");
  PicoConfig  picoConfig;
  uint32_t    configCrc32   = 0;
  std::string parseError;

  if (configJson.empty() || !parsePicoConfig(configJson.c_str(), (int)configJson.size(), picoConfig, parseError)) {
      if (!configJson.empty()) {
          // Corrupt stored config — clear it
          logChannel->send("Config parse failed: " + parseError + " — using fallback");
          persistentStorage.remove("config");
          persistentStorage.flush();
      }
      // Fallback: HID mode, one keyboard
      picoConfig = PicoConfig();
      picoConfig.mode = HID_MODE;
      PicoDeviceConfig kbd;
      kbd.type = PicoDeviceType::KEYBOARD;
      kbd.name = "Keyboard";
      picoConfig.devices.push_back(kbd);
      configCrc32 = 0;  // defined constant — NOT crc32("", 0)
  } else {
      configCrc32 = crc32(configJson.c_str(), configJson.size());
  }

  // Build device manager from PicoConfig
  if (picoConfig.mode == XINPUT_MODE) {
      XinputDeviceManager* xm = new XinputDeviceManager();
      xm->vendorId(picoConfig.vid)
        ->productId(picoConfig.pid)
        ->manufacturer(picoConfig.manufacturer)
        ->productName(picoConfig.product)
        ->serialNumber(picoConfig.serial);
      for (const auto& d : picoConfig.devices) {
          if (d.type == PicoDeviceType::XBOX360_GAMEPAD)
              xm->plugGamepad(d.name.empty() ? std::string("Xbox 360 Controller") : d.name);
      }
      deviceManager = xm;
  } else {
      HidDeviceManager* hm = new HidDeviceManager();
      hm->vendorId(picoConfig.vid)
        ->productId(picoConfig.pid)
        ->manufacturer(picoConfig.manufacturer)
        ->productName(picoConfig.product)
        ->serialNumber(picoConfig.serial);
      for (const auto& d : picoConfig.devices) {
          switch (d.type) {
              case PicoDeviceType::KEYBOARD:
                  hm->plugKeyboard(d.name.empty() ? std::string("Keyboard") : d.name);
                  break;
              case PicoDeviceType::MOUSE:
                  hm->plugMouse(d.name.empty() ? std::string("Mouse") : d.name);
                  break;
              case PicoDeviceType::HID_GAMEPAD:
                  hm->plugGamepad(d.name.empty() ? std::string("Gamepad") : d.name,
                                  d.buttons, d.axesMask, d.hat);
                  break;
              default: break;
          }
      }
      hm->prepareDescriptors();
      deviceManager = hm;
  }
  ```

  Remove the old hardcoded `HidDeviceManager` / `XinputDeviceManager` construction block entirely, including all the hardcoded `plugKeyboard`, `plugMouse`, `plugGamepad` calls.

  **Keep** the `setDeviceManager(deviceManager)` call that immediately follows this block in the current file — it sets the global USB descriptor callback pointer and must not be removed.

- [ ] **Step 4: Update rpcOnBoot call**

  Find `rpcOnBoot(deviceId, bootMode)` at the end of `_main()` and replace with:
  ```cpp
  rpcOnBoot(deviceId, configCrc32);
  ```

- [ ] **Step 5: Update rpcOnBoot signature**

  Find the `rpcOnBoot` function:
  ```cpp
  static bool rpcOnBoot(const std::string& deviceId, DeviceMode bootMode) {
      RpcArg* arg = rpcManager->getRpcArg();
      arg->putString(deviceId.c_str());
      arg->putInt32(static_cast<int32_t>(bootMode));
  ```

  Replace with:
  ```cpp
  static bool rpcOnBoot(const std::string& deviceId, uint32_t configCrc32) {
      RpcArg* arg = rpcManager->getRpcArg();
      arg->putString(deviceId.c_str());
      arg->putInt32(static_cast<int32_t>(configCrc32)); // uint32 sent as int32 bit-pattern
  ```

- [ ] **Step 6: Add M2P_SET_CONFIGURATION handler in initUartRpcSystem()**

  After the existing `M2P_GET_MODE` handler (which will be removed), add:

  ```cpp
  // setConfiguration(string configJson) → bool ok, string errorMsg
  rpc->registerMethod(M2P_SET_CONFIGURATION, [rpc](RpcArg* arg) -> RpcArg* {
      char jsonBuf[960] = {}; // 900 byte limit + headroom
      arg->getString(jsonBuf, sizeof(jsonBuf));
      int jsonLen = (int)strlen(jsonBuf);

      if (jsonLen <= 0 || jsonLen > 900) {
          RpcArg* out = rpc->getRpcArg();
          out->putBool(false);
          out->putString("config too large or empty");
          return out;
      }

      PicoConfig cfg;
      std::string err;
      if (!parsePicoConfig(jsonBuf, jsonLen, cfg, err)) {
          RpcArg* out = rpc->getRpcArg();
          out->putBool(false);
          out->putString(err.c_str());
          return out;
      }

      persistentStorage.put("config", std::string(jsonBuf, jsonLen));
      persistentStorage.flush();

      // Send success reply before rebooting
      RpcArg* out = rpc->getRpcArg();
      out->putBool(true);
      out->putString("");
      // Reboot via channel (same as setMode did)
      rebootChannel->send(false);
      return out;
  });
  ```

- [ ] **Step 7: Remove M2P_SET_MODE and M2P_GET_MODE handlers**

  Delete the `M2P_SET_MODE` and `M2P_GET_MODE` `registerMethod` blocks entirely.

- [ ] **Step 8: Remove test coroutines**

  Remove all the hardcoded keyboard/mouse/gamepad test coroutines (`//keyboard test`, `//mouse test`, `//gamepad 1 test`, etc.). These were tied to hardcoded slot indices that no longer exist.

- [ ] **Step 9: Commit**

  ```bash
  git add Pico/src/mainPico.cpp
  git commit -m "feat: update Pico boot sequence to use PicoConfig with CRC32 handshake"
  ```

---

### Task 15: Update Pico/CMakeLists.txt

**Files:**
- Modify: `Pico/CMakeLists.txt`

- [ ] **Step 1: Add PicoConfig.cpp to the executable**

  Find the `add_executable(InputProxy ...)` block and add `../shared/PicoConfig.cpp` to the list:

  ```cmake
  add_executable(InputProxy
      src/mainPico.cpp
      src/UartManagerPico.cpp
      src/PersistentStorage.cpp
      src/usb_descriptors.cpp
      ../shared/shared.cpp
      ../shared/PicoConfig.cpp
      ../shared/corocgo/corocgo.cpp
      ../shared/corocgo/corocrpc/corocrpc.cpp
      src/devices/AbstractVirtualDevice.cpp
      src/devices/AbstractDeviceManager.cpp
      src/devices/TinyUsbKeyboardDevice.cpp
      src/devices/TinyUsbMouseDevice.cpp
      src/devices/TinyUsbGamepadDevice.cpp
      src/devices/HidDeviceManager.cpp
      src/devices/XInputDevice.cpp
      src/devices/XinputDeviceManager.cpp
      src/devices/tud_driver_xinput.cpp
  )
  ```

- [ ] **Step 2: Build the Pico firmware**

  Build using the Raspberry Pi Pico VS Code Extension or:
  ```bash
  cd Pico
  mkdir -p build && cd build
  cmake .. 2>&1 | tail -5
  cmake --build . 2>&1 | tail -20
  ```
  Expected: no compile errors. Output: `InputProxy.uf2`

- [ ] **Step 3: Flash and test**

  1. Flash `InputProxy.uf2` to the Pico
  2. Flash updated mainboard binary to RPi4
  3. Watch mainboard logs for:
     - `[UART0] onBoot picoId=XXXXX crc=0x00000000` (first boot, no stored config → CRC=0)
     - `[UART0] picoId=XXXXX CRC mismatch — sending setConfiguration`
     - (Pico reboots)
     - `[UART0] onBoot picoId=XXXXX crc=0xNNNNNNNN` (second boot with stored config)
     - `[UART0] picoId=XXXXX config match — board active`
  4. On subsequent reboots: only the "config match" log should appear.

- [ ] **Step 4: Commit**

  ```bash
  git add Pico/CMakeLists.txt
  git commit -m "build: add PicoConfig.cpp to Pico firmware"
  ```

---

## Integration Checklist

Before calling this complete, verify on hardware:

- [ ] First Pico boot (empty flash): CRC=0, mainboard pushes config, Pico reboots, second boot matches
- [ ] Subsequent Pico reboots: CRC matches immediately, board becomes active
- [ ] If mainboard config.json changes: Pico receives new config on next onBoot, reboots, syncs
- [ ] M2P_SET_AXIS still routes correctly after board activation (verify with `emulatedDeviceManager->setAxis(0, ...)`)
- [ ] Unknown picoId: mainboard logs warning, does NOT crash or push config
- [ ] `shared/test_picoconfig.cpp` continues to pass: `g++ -std=c++20 -I. -Ishared shared/PicoConfig.cpp shared/shared.cpp shared/test_picoconfig.cpp -o /tmp/test_pico && /tmp/test_pico`
