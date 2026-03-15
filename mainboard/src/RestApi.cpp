#include "RestApi.h"
#include "CoHttpServer.h"
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

void startRestApi(int port, RealDeviceManager* deviceManager,
                  std::vector<EmulationBoard>* boards) {
    coro([port, deviceManager, boards]() {
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

        router->endpoint("GET", "/emulationboard/{id}/mode",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                int32_t mode = b->getMode();
                std::ostringstream json;
                json << "{\"mode\":\"" << (mode == XINPUT_MODE ? "xinput" : "hid") << "\"}";
                sendJson(session, 200, json.str());
            });

        router->endpoint("POST", "/emulationboard/{id}/mode",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                std::string value = qparam(session, "value");
                if (value != "hid" && value != "xinput") {
                    sendJson(session, 400, "{\"error\":\"value must be 'hid' or 'xinput'\"}");
                    return;
                }
                b->setMode(value == "xinput" ? XINPUT_MODE : HID_MODE);
                sendJson(session, 200, "{\"ok\":true}");
            });

        router->endpoint("POST", "/emulationboard/{id}/plugdevice",
            [boards](coSession session, auto vars) {
                int32_t id;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                int32_t slot = 0, type = 0, hat = 0, axes = 0, buttons = 0;
                try {
                    slot    = std::stoi(qparam(session, "slot",    "0"));
                    type    = std::stoi(qparam(session, "type",    "0"));
                    hat     = std::stoi(qparam(session, "hat",     "0"));
                    axes    = std::stoi(qparam(session, "axes",    "0"));
                    buttons = std::stoi(qparam(session, "buttons", "0"));
                } catch (...) {
                    sendJson(session, 400, "{\"error\":\"invalid params\"}"); return;
                }
                bool ok = b->plugDevice(slot, type, hat, axes, buttons);
                sendJson(session, 200, ok ? "{\"ok\":true}" : "{\"ok\":false}");
            });

        router->endpoint("POST", "/emulationboard/{id}/unplugdevice",
            [boards](coSession session, auto vars) {
                int32_t id, slot;
                if (!parseId(vars, "id", id)) {
                    sendJson(session, 400, "{\"error\":\"invalid id\"}"); return;
                }
                EmulationBoard* b = findBoard(boards, id);
                if (!b)         { sendJson(session, 404, "{\"error\":\"board not found\"}"); return; }
                if (!b->active) { sendJson(session, 503, "{\"error\":\"board not active\"}"); return; }
                try { slot = std::stoi(qparam(session, "slot", "0")); }
                catch (...) { sendJson(session, 400, "{\"error\":\"invalid slot\"}"); return; }
                bool ok = b->unplugDevice(slot);
                sendJson(session, 200, ok ? "{\"ok\":true}" : "{\"ok\":false}");
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
