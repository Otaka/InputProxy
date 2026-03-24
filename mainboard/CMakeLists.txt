cmake_minimum_required(VERSION 3.16)
include_directories(../shared/Corocgo ../shared/ )
project(InputProxyMainBoard CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(app
    ../shared/corocgo/corocgo.cpp
    ../shared/corocgo/corocrpc/corocrpc.cpp
    ../shared/shared.cpp
    ../shared/PicoConfig.cpp
    src/main.cpp
    src/RealDeviceManager.cpp
    src/rest/CoHttpServer.cpp
    src/rest/RestApi.cpp
    src/EmulatedDeviceManager.cpp
    src/VirtualOutputDevice.cpp
    src/MainConfig.cpp
    src/mapping/MappingManager.cpp
    src/mapping/OutputSequenceParser.cpp
    src/mapping/AxisRule.cpp
    src/mapping/LayerManager.cpp
)