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
