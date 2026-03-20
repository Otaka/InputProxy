// mainboard/src/MainboardConfig.cpp
#include "MainboardConfig.h"
#include "shared.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

nlohmann::json parseConfigFile(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f) { std::cerr << "[config] cannot open " << path << "\n"; return {}; }
        return json::parse(f);
    } catch (const json::exception& e) {
        std::cerr << "[config] failed to parse " << path << ": " << e.what() << "\n";
        return {};
    }
}

std::vector<BoardEntry> loadMainboardConfig(const nlohmann::json& root) {
    std::vector<BoardEntry> result;
    try {
        for (const auto& board : root.value("emulation_boards", json::array())) {
            BoardEntry entry;
            entry.picoId                = board.value("id", "");
            entry.config.vid            = board.value("vid", 0x120A);
            entry.config.pid            = board.value("pid", 0x0004);
            entry.config.manufacturer   = board.value("manufacturer", "InputProxy");
            entry.config.product        = board.value("product", "InputProxy Device");
            entry.config.serial         = board.value("serial", "000001");

            for (const auto& dev : board.value("devices", json::array())) {
                std::string typeStr = dev.value("type", "");
                PicoDeviceType type;
                if      (typeStr == "keyboard")       type = PicoDeviceType::KEYBOARD;
                else if (typeStr == "mouse")          type = PicoDeviceType::MOUSE;
                else if (typeStr == "hidgamepad")     type = PicoDeviceType::HID_GAMEPAD;
                else if (typeStr == "xbox360gamepad") type = PicoDeviceType::XBOX360_GAMEPAD;
                else { std::cerr << "[config] unknown device type: " << typeStr << "\n"; continue; }

                PicoDeviceConfig d;
                d.type     = type;
                d.name     = dev.value("name", "");
                d.buttons  = dev.value("buttons",  (uint8_t)0);
                d.axesMask = dev.value("axesmask", (uint8_t)0);
                d.hat      = dev.value("hat", false);

                std::string devId = dev.value("id", "");
                entry.deviceIds.push_back(devId.empty() ? "dev_" + std::to_string(entry.config.devices.size()) : devId);
                entry.config.devices.push_back(d);
            }

            bool hasXInput = false;
            for (const auto& d : entry.config.devices)
                if (d.type == PicoDeviceType::XBOX360_GAMEPAD) hasXInput = true;
            entry.config.mode = hasXInput ? XINPUT_MODE : HID_MODE;

            if (!entry.picoId.empty() && !entry.config.devices.empty())
                result.push_back(std::move(entry));
        }
    } catch (const json::exception& e) {
        std::cerr << "[config] failed to parse config: " << e.what() << "\n";
        return {};
    }
    return result;
}

std::vector<VirtualOutputDevice> buildVirtualDevices(const BoardEntry& entry) {
    std::vector<VirtualOutputDevice> result;
    for (size_t i = 0; i < entry.config.devices.size(); i++) {
        const auto& d = entry.config.devices[i];
        VirtualOutputDevice vd;
        vd.id        = (i < entry.deviceIds.size()) ? entry.deviceIds[i] : "dev_" + std::to_string(i);
        vd.slotIndex = (int)i;
        vd.type      = d.type;
        vd.board     = nullptr; // filled in by EmulatedDeviceManager::registerBoard
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
