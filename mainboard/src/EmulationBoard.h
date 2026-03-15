#pragma once

#include <string>
#include <cstdint>
#include "UartManager.h"
#include "rpcinterface.h"
#include "../shared/shared.h"
#include "Corocgo/corocrpc/corocrpc.h"

class EmulationBoard {
public:
    int id;
    std::string serialString;
    corocrpc::RpcManager* rpc;
    UART_CHANNEL uartChannel;
    bool active;

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

    void setMode(int32_t mode) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putInt32(mode);
        corocrpc::RpcResult result = rpc->call(M2P_SET_MODE, arg);
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
    }

    int32_t getMode() {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        corocrpc::RpcResult result = rpc->call(M2P_GET_MODE, arg);
        int32_t mode = -1;
        if (result.arg != nullptr) {
            mode = result.arg->getInt32();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return mode;
    }

    bool plugDevice(int32_t slotIndex, int32_t deviceType, int32_t hat, int32_t axesMask, int32_t buttons) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putInt32(slotIndex);
        arg->putInt32(deviceType);
        arg->putInt32(hat);
        arg->putInt32(axesMask);
        arg->putInt32(buttons);
        corocrpc::RpcResult result = rpc->call(M2P_PLUG_DEVICE, arg);
        bool ok = false;
        if (result.arg != nullptr) {
            ok = result.arg->getBool();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return ok;
    }

    bool unplugDevice(int32_t slotIndex) {
        corocrpc::RpcArg* arg = rpc->getRpcArg();
        arg->putInt32(slotIndex);
        corocrpc::RpcResult result = rpc->call(M2P_UNPLUG_DEVICE, arg);
        bool ok = false;
        if (result.arg != nullptr) {
            ok = result.arg->getBool();
        }
        rpc->disposeRpcResult(result);
        rpc->disposeRpcArg(arg);
        return ok;
    }
};
