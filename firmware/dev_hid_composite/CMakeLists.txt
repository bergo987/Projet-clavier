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

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

project(dev_hid_composite C CXX ASM)

pico_sdk_init()

add_executable(dev_hid_composite)

target_sources(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

target_include_directories(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

# ── Libraries ────────────────────────────────────────────────────────────────
# Ajout de hardware_flash et hardware_sync pour la sauvegarde des touches.
# Regroupement de toutes les libs sous PUBLIC pour éviter les conflits.
target_link_libraries(dev_hid_composite PUBLIC
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
        hardware_i2c        # OLED
        hardware_adc        # Courant ADC
        hardware_pio        # WS2812
        hardware_flash      # Sauvegarde en mémoire Flash
        hardware_sync       # Gestion des interruptions pendant l'écriture Flash
)

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