#pragma once

#include "rpcinterface.h"

// Initialize web server for HID device testing
// Takes the RPC client to communicate with the Pico
void initWebserver(Main2Pico& main2PicoRpcClient);
