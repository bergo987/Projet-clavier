# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dev_hid_composite C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(dev_hid_composite)

target_sources(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

# ── Libraries ────────────────────────────────────────────────────────────────
# Kept your original USB/PIO libs, added hardware_i2c and hardware_adc for
# the SSD1306 OLED (LCD-22495) and the current-sensing ADC input.
target_link_libraries(dev_hid_composite PUBLIC
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
        hardware_i2c       # ← SSD1306 OLED over I2C
        hardware_adc       # ← current sensing via ADC pin
)
target_link_libraries(dev_hid_composite PRIVATE
        pico_stdlib
        hardware_pio       # ← WS2812 LED strip
)

# Uncomment to enable fix for Errata RP2040-E5 (requires GPIO 15)
#target_compile_definitions(dev_hid_composite PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

# ── WS2812 PIO header generation ─────────────────────────────────────────────
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

pico_generate_pio_header(dev_hid_composite
        ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

add_custom_target(dev_hid_composite_datasheet
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND pioasm -o python
                ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
                ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)

add_dependencies(dev_hid_composite dev_hid_composite_datasheet)

pico_add_extra_outputs(dev_hid_composite)