#pragma once

#include <string>
#include <cstdint>
#include "UartManager.h"
#include "../shared/rpcinterface.h"
#include "../shared/shared.h"
#include "../shared/PicoConfig.h"
#include "../shared/crc32.h"
#include "Corocgo/corocrpc/corocrpc.h"

class EmulationBoard {
public:
    int id;
    std::string serialString;
    corocrpc::RpcManager* rpc;
    UART_CHANNEL uartChannel;
    bool active;

    PicoConfig picoConfig;  // the config this board should be running

    // ── Main → Pico RPC calls ─────────────────────────────────────────────

    int pingPico(int32_t val) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putInt32(val);
        corocrpc::RpcResult result = rpc->call(M2P_PING, arg);
        int pingResult = -1;
        if (result.arg != nullptr) {
            pingResult = result.arg->getInt32();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return pingResult;
    }

    void setLed(bool state) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putBool(state);
        corocrpc::RpcResult result = rpc->call(M2P_SET_LED, arg);
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
    }

    bool getLedStatus() {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        corocrpc::RpcResult result = rpc->call(M2P_GET_LED_STATUS, arg);
        bool state = false;
        if (result.arg != nullptr) {
            state = result.arg->getBool();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return state;
    }

    bool rebootFlashMode() {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        corocrpc::RpcResult result = rpc->call(M2P_REBOOT_FLASH_MODE, arg);
        bool ok = false;
        if (result.arg != nullptr) {
            ok = result.arg->getBool();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return ok;
    }

    void reboot() {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        rpc->call(M2P_REBOOT, arg);
        rpc->disposeRpcArg(arg);
    }

    void setAxis(int32_t device, int32_t axis, int32_t value) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putInt32(device);
        arg->putInt32(axis);
        arg->putInt32(value);
        corocrpc::RpcResult result = rpc->call(M2P_SET_AXIS, arg);
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
    }

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
};
