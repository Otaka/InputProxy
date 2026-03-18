#include "RestApi.h"
#include "CoHttpServer.h"
#include "EmulatedDeviceManager.h"
#include "PicoConfig.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

using namespace corocgo;

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

static void sendJson(coSession session, int status, const std::string& json) {
    session->setStatus(status);
    session->setResponseHeader("Content-Type", "application/json");
    session->setResponseHeader("Access-Control-Allow-Origin", "*");
    session->write(json.c_str(), static_cast<int>(json.size()));
    session->close();
}

static std::string qparam(const coSession& session, const std::string& key,
                          const std::string& def = "") {
    auto it = session->queryString.find(key);
    return it != session->queryString.end() ? it->second : def;
}

static EmulationBoard* findBoard(std::vector<EmulationBoard>* boards, int id) {
    for (auto& b : *boards)
        if (b.id == id) return &b;
    return nullptr;
}

static std::string boardJson(const EmulationBoard& b) {
    std::ostringstream j;
    j << "{"
      << "\"id\":"            << b.id                              << ","
      << "\"serialString\":\"" << jsonEscape(b.serialString)      << "\","
      << "\"uartChannel\":"   << b.uartChannel                    << ","
      << "\"active\":"        << (b.active ? "true" : "false")
      << "}";
    return j.str();
}

// Parse an integer from path vars or query string; returns false on failure.
static bool parseId(const std::map<std::string, std::string>& vars,
                    const std::string& key, int32_t& out) {
    auto it = vars.find(key);
    if (it == vars.end()) return false;
    try { out = std::stoi(it->second); return true; }
    catch (...) { return false; }
}

// ---------------------------------------------------------------------------

static std::string picoDeviceTypeStr(PicoDeviceType t) {
    switch (t) {
        case PicoDeviceType::KEYBOARD:       return "keyboard";
        case PicoDeviceType::MOUSE:          return "mouse";
        case PicoDeviceType::HID_GAMEPAD:    return "hid_gamepad";
        case PicoDeviceType::XBOX360_GAMEPAD:return "xbox360_gamepad";
        default:                             return "unknown";
    }
}

void startRestApi(int port, RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards,
                  EmulatedDeviceManager* emulatedDeviceManager,
                  LayerManager* layerManager) {
    coro([port, deviceManager, boards, emulatedDeviceManager, layerManager]() {
        auto router = std::make_shared<CoHttpRouter>();

        // ---- /emulationboard/* ----

        router->endpoint("GET", "/emulationboard/list",
            [boards](coSession session, auto) {
                std::ostringstream json;
                json << "[";
                bool first = true;
                for (auto& b : *boards) {
                    if (!first) json << ",";
                    first = false;
                    json << boardJson(b);
                }
                json << "]";
                sendJson(session, 200, json.str());
            });

        router->endpoint("GET", "/emulationboard/{id}",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b) { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                sendJson(session, 200, boardJson(*b));
            });

        router->endpoint("GET", "/emulationboard/{id}/devices",
            [boards, emulatedDeviceManager](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b) { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                std::ostringstream json;
                json << "[";
                bool first = true;
                for (auto& d : emulatedDeviceManager->getDevices()) {
                    if (d.board != b) continue;
                    if (!first) json << ",";
                    first = false;
                    json << "{"
                         << "\"id\":\""     << jsonEscape(d.id)          << "\","
                         << "\"slotIndex\":"<< d.slotIndex               << ","
                         << "\"type\":\""   << picoDeviceTypeStr(d.type) << "\","
                         << "\"axes\":[";
                    bool firstAxis = true;
                    for (auto& a : d.axisTable.getEntries()) {
                        if (!firstAxis) json << ",";
                        firstAxis = false;
                        json << "{\"name\":\"" << jsonEscape(a.name) << "\","
                             << "\"index\":"   << a.index            << "}";
                    }
                    json << "]}";
                }
                json << "]";
                sendJson(session, 200, json.str());
            });

        router->endpoint("POST", "/emulationboard/{id}/reboot",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                bool flash = qparam(session, "flash") == "true";
                if (flash) {
                    bool ok = b->rebootFlashMode();
                    sendJson(session, 200, ok ? "{\"ok\":true}" : "{\"ok\":false}");
                } else {
                    b->reboot();
                    sendJson(session, 200, "{\"ok\":true}");
                }
            });

        router->endpoint("POST", "/emulationboard/{id}/ping",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                int32_t val = 0;
                try { val = std::stoi(qparam(session, "value", "0")); } catch (...) {}
                int result = b->pingPico(val);
                std::ostringstream json;
                json << "{\"result\":" << result << "}";
                sendJson(session, 200, json.str());
            });

        router->endpoint("POST", "/emulationboard/{id}/led",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                b->setLed(qparam(session, "value") == "true");
                sendJson(session, 200, "{\"ok\":true}");
            });

        router->endpoint("GET", "/emulationboard/{id}/led",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                bool state = b->getLedStatus();
                sendJson(session, 200, state ? "{\"value\":true}" : "{\"value\":false}");
            });

        router->endpoint("POST", "/emulationboard/{id}/setaxis",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                int32_t device = 0, axis = 0, value = 0;
                try {
                    device = std::stoi(qparam(session, "device", "0"));
                    axis   = std::stoi(qparam(session, "axis",   "0"));
                    value  = std::stoi(qparam(session, "value",  "0"));
                } catch (...) {
                    sendJson(session, 400, "{\"error\":\"invalid params\"}"); return;
                }
                b->setAxis(device, axis, value);
                sendJson(session, 200, "{\"ok\":true}");
            });

        // ---- /realdevices/* ----

        router->endpoint("GET", "/realdevices/list",
            [deviceManager](coSession session, auto) {
                std::ostringstream json;
                json << "[";
                bool first = true;
                for (auto& [id, dev] : deviceManager->getDevices()) {
                    if (!first) json << ",";
                    first = false;
                    json << "{"
                         << "\"deviceId\":"    << dev.deviceId                           << ","
                         << "\"deviceIdStr\":\"" << jsonEscape(dev.deviceIdStr)          << "\","
                         << "\"evdevPath\":\""  << jsonEscape(dev.evdevPath)             << "\","
                         << "\"active\":"       << (dev.active ? "true" : "false")       << ","
                         << "\"serial\":\""     << jsonEscape(dev.serial)                << "\","
                         << "\"usbPath\":\""    << jsonEscape(dev.usbPath)               << "\","
                         << "\"deviceName\":\"" << jsonEscape(dev.deviceName)            << "\""
                         << "}";
                }
                json << "]";
                sendJson(session, 200, json.str());
            });

        router->endpoint("GET", "/realdevices/detailed/{deviceId}",
            [deviceManager](coSession session, auto vars) {
                unsigned int deviceId = 0;
                try { deviceId = std::stoul(vars.at("deviceId")); }
                catch (...) {
                    sendJson(session, 400, "{\"error\":\"invalid deviceId\"}"); return;
                }
                RealDevice* dev = deviceManager->getDevice(deviceId);
                if (!dev) { sendJson(session, 404, "{\"error\":\"device not found\"}"); return; }

                std::ostringstream json;
                json << "{"
                     << "\"deviceId\":"    << dev->deviceId                           << ","
                     << "\"deviceIdStr\":\"" << jsonEscape(dev->deviceIdStr)          << "\","
                     << "\"evdevPath\":\""  << jsonEscape(dev->evdevPath)             << "\","
                     << "\"active\":"       << (dev->active ? "true" : "false")       << ","
                     << "\"serial\":\""     << jsonEscape(dev->serial)                << "\","
                     << "\"usbPath\":\""    << jsonEscape(dev->usbPath)               << "\","
                     << "\"deviceName\":\"" << jsonEscape(dev->deviceName)            << "\","
                     << "\"axes\":[";
                bool first = true;
                for (auto& entry : dev->axes.getEntries()) {
                    if (!first) json << ",";
                    first = false;
                    json << "{\"name\":\"" << jsonEscape(entry.name) << "\","
                         << "\"index\":"   << entry.index            << "}";
                }
                json << "]}";
                sendJson(session, 200, json.str());
            });

        // ---- /layers/* ----

        router->endpoint("GET", "/layers",
            [layerManager](coSession session, auto) {
                std::ostringstream json;
                json << "[";
                bool first = true;
                for (const auto& layer : layerManager->allLayers) {
                    bool active = std::find(layerManager->activeStack.begin(),
                                            layerManager->activeStack.end(),
                                            const_cast<Layer*>(&layer))
                                  != layerManager->activeStack.end();
                    if (!first) json << ",";
                    first = false;
                    json << "{"
                         << "\"id\":\""   << jsonEscape(layer.id)   << "\","
                         << "\"name\":\"" << jsonEscape(layer.name) << "\","
                         << "\"active\":" << (active ? "true" : "false")
                         << "}";
                }
                json << "]";
                sendJson(session, 200, json.str());
            });

        router->endpoint("GET", "/layers/active",
            [layerManager](coSession session, auto) {
                std::ostringstream json;
                json << "[";
                bool first = true;
                for (const auto* layer : layerManager->activeStack) {
                    if (!first) json << ",";
                    first = false;
                    json << "{"
                         << "\"id\":\""   << jsonEscape(layer->id)   << "\","
                         << "\"name\":\"" << jsonEscape(layer->name) << "\""
                         << "}";
                }
                json << "]";
                sendJson(session, 200, json.str());
            });

        router->endpoint("POST", "/layers/{id}/activate",
            [layerManager](coSession session, auto vars) {
                auto it = vars.find("id");
                if (it == vars.end()) {
                    sendJson(session, 400, "{\"error\":\"missing id\"}"); return;
                }
                if (!layerManager->findLayer(it->second)) {
                    sendJson(session, 404, "{\"error\":\"layer not found\"}"); return;
                }
                layerManager->activate(it->second);
                sendJson(session, 200, "{\"ok\":true}");
            });

        router->endpoint("POST", "/layers/{id}/deactivate",
            [layerManager](coSession session, auto vars) {
                auto it = vars.find("id");
                if (it == vars.end()) {
                    sendJson(session, 400, "{\"error\":\"missing id\"}"); return;
                }
                if (!layerManager->findLayer(it->second)) {
                    sendJson(session, 404, "{\"error\":\"layer not found\"}"); return;
                }
                layerManager->deactivate(it->second);
                sendJson(session, 200, "{\"ok\":true}");
            });

        // ---- Server loop ----

        coServer server = createServer(port);
        std::cout << "[HTTP] Listening on port " << port << std::endl;

        while (true) {
            coSession session = server->accept();
            if (!session || server->isClosed())
                break;
            coro([session, router]() {
                router->dispatch(session);
            });
        }
        server->free();
    });
}
