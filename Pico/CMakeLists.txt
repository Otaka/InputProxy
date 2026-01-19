# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(InputProxy C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(InputProxy
    src/mainPico.cpp
    src/UartManagerPico.cpp
   # src/UsbCdcManager.cpp
   # src/devices/usbCdc.cpp
    src/devices/AbstractVirtualDevice.cpp
   # src/devices/KeyboardDevice.cpp
    src/devices/TinyUsbKeyboardDevice.cpp
    src/devices/TinyUsbMouseDevice.cpp
    src/devices/TinyUsbGamepadDevice.cpp
    src/devices/DeviceManager.cpp
    src/Timer.cpp
)

pico_set_program_name(InputProxy "InputProxy")
pico_set_program_version(InputProxy "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(InputProxy 0)
pico_enable_stdio_usb(InputProxy 0)

# Add the standard library to the build
target_link_libraries(InputProxy
        pico_stdlib
        pico_cyw43_arch_none
        tinyusb_device
        tinyusb_board)

# Add the standard include files to the build
target_include_directories(InputProxy PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
)

# Add any user requested libraries
target_link_libraries(InputProxy 
        
        )

pico_add_extra_outputs(InputProxy)

