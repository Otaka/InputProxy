// mainboard/src/MainConfig.cpp
#include "MainConfig.h"
#include "shared.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// ── Global ────────────────────────────────────────────────────────────────────

ConfRoot gConfig;

// ── from_json helpers ─────────────────────────────────────────────────────────

static PicoDeviceType parsePicoDeviceType(const std::string& s, std::vector<std::string>& errors) {
    if (s == "keyboard")       return PicoDeviceType::KEYBOARD;
    if (s == "mouse")          return PicoDeviceType::MOUSE;
    if (s == "hidgamepad")     return PicoDeviceType::HID_GAMEPAD;
    if (s == "xbox360gamepad") return PicoDeviceType::XBOX360_GAMEPAD;
    errors.push_back("unknown device type: \"" + s + "\"");
    return PicoDeviceType::HID_GAMEPAD;  // placeholder; config will be rejected
}

static std::string serializePicoDeviceType(PicoDeviceType t) {
    switch (t) {
        case PicoDeviceType::KEYBOARD:        return "keyboard";
        case PicoDeviceType::MOUSE:           return "mouse";
        case PicoDeviceType::HID_GAMEPAD:     return "hidgamepad";
        case PicoDeviceType::XBOX360_GAMEPAD: return "xbox360gamepad";
    }
    return "hidgamepad";
}

static ConfVod confVodFromJson(const json& j, std::vector<std::string>& errors) {
    ConfVod v;
    v.id       = j.value("id", "");
    v.type     = parsePicoDeviceType(j.value("type", ""), errors);
    v.name     = j.value("name", "");
    v.buttons  = j.value("buttons",  (uint8_t)0);
    v.axesMask = j.value("axesmask", (uint8_t)0);   // JSON key: axesmask
    v.hat      = j.value("hat", false);
    return v;
}

static json confVodToJson(const ConfVod& v) {
    return json{
        {"id",       v.id},
        {"type",     serializePicoDeviceType(v.type)},
        {"name",     v.name},
        {"buttons",  v.buttons},
        {"axesmask", v.axesMask},
        {"hat",      v.hat}
    };
}

static ConfEmulationBoard confEmulationBoardFromJson(const json& j, std::vector<std::string>& errors) {
    ConfEmulationBoard b;
    b.id           = j.value("id", "");
    b.vid          = j.value("vid", (uint16_t)0x120A);
    b.pid          = j.value("pid", (uint16_t)0x0004);
    b.manufacturer = j.value("manufacturer", "InputProxy");
    b.product      = j.value("product", "InputProxy Device");
    b.serial       = j.value("serial", "000001");
    for (const auto& d : j.value("devices", json::array()))
        b.devices.push_back(confVodFromJson(d, errors));
    return b;
}

static json confEmulationBoardToJson(const ConfEmulationBoard& b) {
    json devs = json::array();
    for (const auto& d : b.devices) devs.push_back(confVodToJson(d));
    return json{
        {"id",           b.id},
        {"vid",          b.vid},
        {"pid",          b.pid},
        {"manufacturer", b.manufacturer},
        {"product",      b.product},
        {"serial",       b.serial},
        {"devices",      devs}
    };
}

static ConfVid confVidFromJson(const json& j) {
    return { j.value("id", ""), j.value("name", "") };
}

static json confVidToJson(const ConfVid& v) {
    return json{ {"id", v.id}, {"name", v.name} };
}

static ConfRealDevice confRealDeviceFromJson(const json& j) {
    ConfRealDevice r;
    r.id         = j.value("id", "");
    r.assignedTo = j.value("assignedTo", "");
    auto it = j.find("rename_axes");              // JSON key: rename_axes
    if (it != j.end() && it->is_object())
        for (auto kt = it->begin(); kt != it->end(); ++kt)
            if (kt.value().is_string())
                r.renameAxes[kt.key()] = kt.value().get<std::string>();
    return r;
}

static json confRealDeviceToJson(const ConfRealDevice& r) {
    json renames = json::object();
    for (const auto& [k, v] : r.renameAxes) renames[k] = v;
    return json{
        {"id",          r.id},
        {"assignedTo",  r.assignedTo},
        {"rename_axes", renames}
    };
}

static ConfAxisEntry confAxisEntryFromJson(const json& j) {
    return { j.value("from", ""), j.value("to", "") };
}

static json confAxisEntryToJson(const ConfAxisEntry& a) {
    return json{ {"from", a.from}, {"to", a.to} };
}

static ConfActionType parseConfActionType(const std::string& s, std::vector<std::string>& errors) {
    if (s == "emit_axis")       return ConfActionType::EmitAxis;
    if (s == "output_sequence") return ConfActionType::OutputSequence;
    if (s == "sleep")           return ConfActionType::Sleep;
    errors.push_back("unknown action type: \"" + s + "\"");
    return ConfActionType::EmitAxis;  // placeholder; config will be rejected
}

static std::string serializeConfActionType(ConfActionType t) {
    switch (t) {
        case ConfActionType::EmitAxis:        return "emit_axis";
        case ConfActionType::OutputSequence:  return "output_sequence";
        case ConfActionType::Sleep:           return "sleep";
    }
    return "emit_axis";
}

static ConfAction confActionFromJson(const json& j, std::vector<std::string>& errors) {
    ConfAction a;
    a.type     = parseConfActionType(j.value("type", ""), errors);
    a.vod      = j.value("vod", "");
    a.axis     = j.value("axis", "");
    a.sequence = j.value("sequence", "");
    a.timeMs   = j.value("time", 0);
    return a;
}

static json confActionToJson(const ConfAction& a) {
    return json{
        {"type",     serializeConfActionType(a.type)},
        {"vod",      a.vod},
        {"axis",     a.axis},
        {"sequence", a.sequence},
        {"time",     a.timeMs}
    };
}

static std::vector<ConfAction> parseActionArray(const json& j, std::vector<std::string>& errors) {
    std::vector<ConfAction> result;
    if (!j.is_array()) return result;
    for (const auto& a : j) result.push_back(confActionFromJson(a, errors));
    return result;
}

static ConfRule confRuleFromJson(const json& j, std::vector<std::string>& errors) {
    ConfRule r;
    std::string typeStr = j.value("type", "");
    if (typeStr == "hotkey") {
        r.type = ConfRuleType::Hotkey;
    } else if (typeStr == "simple") {
        r.type = ConfRuleType::Simple;
    } else if (typeStr == "block") {
        r.type = ConfRuleType::Block;
        for (const auto& ax : j.value("axes", json::array())) {
            ConfBlockAxis ba;
            ba.axis  = ax.value("axis", "");
            ba.value = ax.value("value", 0);
            if (!ba.axis.empty()) r.blockAxes.push_back(ba);
        }
    } else if (typeStr == "vod_state") {
        r.type = ConfRuleType::VodState;
        std::string stateStr = j.value("state", "active");
        if      (stateStr == "silenced")     r.vodState = ConfVodState::Silenced;
        else if (stateStr == "disconnected") r.vodState = ConfVodState::Disconnected;
        else                                 r.vodState = ConfVodState::Active;
    } else if (typeStr == "turbo") {
        r.type = ConfRuleType::Turbo;
        r.turboAxis         = j.value("axis", "");
        r.turboOnMs         = j.value("on_ms", 100);
        r.turboOffMs        = j.value("off_ms", 100);
        r.turboInitialDelay = j.value("initial_delay_ms", 0);
        r.turboMaxValue     = j.value("max_value", 1000);
        r.turboMinValue     = j.value("min_value", 0);
        std::string cond    = j.value("condition", "while_axis_active");
        r.turboCondition    = (cond == "always")
                            ? ConfTurboCondition::Always
                            : ConfTurboCondition::WhileAxisActive;
    } else {
        errors.push_back("unknown rule type: \"" + typeStr + "\"");
    }
    r.vid       = j.value("vid", "");
    r.vod       = j.value("vod", "");
    r.hotkey    = j.value("hotkey", "");
    r.propagate = j.value("propagate", false);
    if (r.type == ConfRuleType::Simple) {
        for (const auto& a : j.value("axes", json::array()))
            r.axes.push_back(confAxisEntryFromJson(a));
    }
    auto pressIt   = j.find("press_action");
    auto releaseIt = j.find("release_action");
    if (pressIt   != j.end()) r.pressActions   = parseActionArray(*pressIt,   errors);
    if (releaseIt != j.end()) r.releaseActions = parseActionArray(*releaseIt, errors);
    return r;
}

static json confRuleToJson(const ConfRule& r) {
    json axes = json::array();
    for (const auto& a : r.axes) axes.push_back(confAxisEntryToJson(a));
    json press = json::array();
    for (const auto& a : r.pressActions) press.push_back(confActionToJson(a));
    json release = json::array();
    for (const auto& a : r.releaseActions) release.push_back(confActionToJson(a));
    return json{
        {"type",           r.type == ConfRuleType::Hotkey ? "hotkey" : "simple"},
        {"vid",            r.vid},
        {"vod",            r.vod},
        {"hotkey",         r.hotkey},
        {"propagate",      r.propagate},
        {"axes",           axes},
        {"press_action",   press},
        {"release_action", release}
    };
}

static ConfLayer confLayerFromJson(const json& j, std::vector<std::string>& errors) {
    ConfLayer l;
    l.id     = j.value("id", "");
    l.name   = j.value("name", "");
    l.active = j.value("active", true);
    for (const auto& r : j.value("rules", json::array()))
        l.rules.push_back(confRuleFromJson(r, errors));
    auto actIt = j.find("activation");
    if (actIt != j.end()) {
        ConfActivation act;
        std::string modeStr = actIt->value("mode", "toggle");
        if      (modeStr == "while_active")     act.mode = ConfActivationMode::WhileActive;
        else if (modeStr == "while_not_active") act.mode = ConfActivationMode::WhileNotActive;
        else                                    act.mode = ConfActivationMode::Toggle;
        act.vid    = actIt->value("vid", "");
        act.hotkey = actIt->value("hotkey", "");
        l.activation = act;
    }
    return l;
}

static json confLayerToJson(const ConfLayer& l) {
    json rules = json::array();
    for (const auto& r : l.rules) rules.push_back(confRuleToJson(r));
    return json{
        {"id",     l.id},
        {"name",   l.name},
        {"active", l.active},
        {"rules",  rules}
    };
}

// ── loadConfig / saveConfig ───────────────────────────────────────────────────

bool loadConfig(const std::string& path, std::vector<std::string>& errors) {
    try {
        std::ifstream f(path);
        if (!f) {
            errors.push_back("cannot open file: " + path);
            std::cerr << "[config] cannot open " << path << "\n";
            return false;
        }
        json root = json::parse(f, nullptr, true, true);

        ConfRoot local;
        for (const auto& b : root.value("emulation_boards",      json::array()))
            local.emulationBoards.push_back(confEmulationBoardFromJson(b, errors));
        for (const auto& v : root.value("virtual_input_devices", json::array()))
            local.vids.push_back(confVidFromJson(v));
        for (const auto& r : root.value("real_devices",          json::array()))
            local.realDevices.push_back(confRealDeviceFromJson(r));
        for (const auto& l : root.value("layers",                json::array()))
            local.layers.push_back(confLayerFromJson(l, errors));

        if (!errors.empty()) {
            for (const auto& e : errors)
                std::cerr << "[config] error: " << e << "\n";
            std::cerr << "[config] rejected " << path << " — " << errors.size() << " error(s)\n";
            return false;
        }

        gConfig = std::move(local);
        return true;
    } catch (const json::exception& e) {
        errors.push_back(std::string("JSON parse error: ") + e.what());
        std::cerr << "[config] parse error: " << e.what() << "\n";
        return false;
    }
}

bool saveConfig(const std::string& path) {
    try {
        json root;
        json boards = json::array();
        for (const auto& b : gConfig.emulationBoards) boards.push_back(confEmulationBoardToJson(b));
        json vids = json::array();
        for (const auto& v : gConfig.vids) vids.push_back(confVidToJson(v));
        json rdevs = json::array();
        for (const auto& r : gConfig.realDevices) rdevs.push_back(confRealDeviceToJson(r));
        json layers = json::array();
        for (const auto& l : gConfig.layers) layers.push_back(confLayerToJson(l));

        root["emulation_boards"]      = boards;
        root["virtual_input_devices"] = vids;
        root["real_devices"]          = rdevs;
        root["layers"]                = layers;

        std::ofstream f(path);
        if (!f) {
            std::cerr << "[config] cannot write " << path << "\n";
            return false;
        }
        f << root.dump(4) << "\n";
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[config] save error: " << e.what() << "\n";
        return false;
    }
}

// ── buildBoardEntries ─────────────────────────────────────────────────────────

std::vector<BoardEntry> buildBoardEntries(const std::vector<ConfEmulationBoard>& boards) {
    std::vector<BoardEntry> result;
    for (const auto& b : boards) {
        BoardEntry entry;
        entry.picoId              = b.id;
        entry.config.vid          = b.vid;
        entry.config.pid          = b.pid;
        entry.config.manufacturer = b.manufacturer;
        entry.config.product      = b.product;
        entry.config.serial       = b.serial;

        bool hasXInput = false;
        for (const auto& d : b.devices) {
            PicoDeviceConfig dc;
            dc.type     = d.type;
            dc.name     = d.name;
            dc.buttons  = d.buttons;
            dc.axesMask = d.axesMask;
            dc.hat      = d.hat;
            entry.deviceIds.push_back(d.id.empty()
                ? "dev_" + std::to_string(entry.config.devices.size()) : d.id);
            entry.config.devices.push_back(dc);
            if (d.type == PicoDeviceType::XBOX360_GAMEPAD) hasXInput = true;
        }
        if (hasXInput) {
            entry.config.mode         = XINPUT_MODE;
            entry.config.vid          = 0x045E;  // Microsoft
            entry.config.pid          = 0x028E;  // Xbox 360 Controller
            entry.config.manufacturer = "Microsoft";
            entry.config.product      = "Xbox 360 Controller";
        } else {
            entry.config.mode = HID_MODE;
        }

        if (!entry.picoId.empty() && !entry.config.devices.empty())
            result.push_back(std::move(entry));
    }
    return result;
}

// ── buildVirtualDevices (moved unchanged from MainboardConfig.cpp) ────────────

std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry) {
    std::vector<VirtualOutputDevice> result;
    for (size_t i = 0; i < entry.config.devices.size(); i++) {
        const auto& d = entry.config.devices[i];
        VirtualOutputDevice vd;
        vd.id        = (i < entry.deviceIds.size()) ? entry.deviceIds[i] : "dev_" + std::to_string(i);
        vd.slotIndex = (int)i;
        vd.type      = d.type;
        vd.board     = nullptr;
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
