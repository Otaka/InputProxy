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
