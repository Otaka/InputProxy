#include "RestApi.h"
#include "CoHttpServer.h"
#include <iostream>
#include <sstream>

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

static void sendJson(coSession& session, int status, const std::string& json) {
    session->setStatus(status);
    session->setResponseHeader("Content-Type", "application/json");
    session->setResponseHeader("Access-Control-Allow-Origin", "*");
    session->write(json.c_str(), static_cast<int>(json.size()));
    session->close();
}

static void handleRequest(coSession session, RealDeviceManager& deviceManager) {
    const std::string& path = session->path;

    // GET /realdevices/list
    if (path == "/realdevices/list") {
        std::ostringstream json;
        json << "[";
        bool first = true;
        for (auto& [id, dev] : deviceManager.getDevices()) {
            if (!first) json << ",";
            first = false;
            json << "{"
                 << "\"deviceId\":" << dev.deviceId << ","
                 << "\"deviceIdStr\":\"" << jsonEscape(dev.deviceIdStr) << "\","
                 << "\"evdevPath\":\"" << jsonEscape(dev.evdevPath) << "\","
                 << "\"active\":" << (dev.active ? "true" : "false") << ","
                 << "\"serial\":\"" << jsonEscape(dev.serial) << "\","
                 << "\"usbPath\":\"" << jsonEscape(dev.usbPath) << "\","
                 << "\"deviceName\":\"" << jsonEscape(dev.deviceName) << "\""
                 << "}";
        }
        json << "]";
        sendJson(session, 200, json.str());
        return;
    }

    // GET /realdevices/detailed/{deviceId}
    const std::string prefix = "/realdevices/detailed/";
    if (path.rfind(prefix, 0) == 0) {
        std::string idStr = path.substr(prefix.size());
        unsigned int deviceId = 0;
        try { deviceId = std::stoul(idStr); }
        catch (...) {
            sendJson(session, 400, "{\"error\":\"invalid deviceId\"}");
            return;
        }

        RealDevice* dev = deviceManager.getDevice(deviceId);
        if (!dev) {
            sendJson(session, 404, "{\"error\":\"device not found\"}");
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"deviceId\":" << dev->deviceId << ","
             << "\"deviceIdStr\":\"" << jsonEscape(dev->deviceIdStr) << "\","
             << "\"evdevPath\":\"" << jsonEscape(dev->evdevPath) << "\","
             << "\"active\":" << (dev->active ? "true" : "false") << ","
             << "\"serial\":\"" << jsonEscape(dev->serial) << "\","
             << "\"usbPath\":\"" << jsonEscape(dev->usbPath) << "\","
             << "\"deviceName\":\"" << jsonEscape(dev->deviceName) << "\","
             << "\"axes\":[";

        bool first = true;
        for (auto& entry : dev->axes.getEntries()) {
            if (!first) json << ",";
            first = false;
            json << "{\"name\":\"" << jsonEscape(entry.name) << "\","
                 << "\"index\":" << entry.index << "}";
        }
        json << "]}";
        sendJson(session, 200, json.str());
        return;
    }

    // 404 for unknown routes
    sendJson(session, 404, "{\"error\":\"not found\"}");
}

void startRestApi(int port, RealDeviceManager& deviceManager) {
    coro([port, &deviceManager]() {
        coServer server = createServer(port);
        std::cout << "[HTTP] Listening on port " << port << std::endl;

        while (true) {
            coSession session = server->accept();
            if (!session || server->isClosed()) break;

            coro([session, &deviceManager]() {
                handleRequest(session, deviceManager);
            });
        }
        server->free();
    });
}
