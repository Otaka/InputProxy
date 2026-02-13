cmake_minimum_required(VERSION 3.16)
include_directories(../shared/)
project(InputProxyMainBoard CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(app src/EventLoop.cpp src/main.cpp src/webtester.cpp src/RealDeviceManager.cpp)