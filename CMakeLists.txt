cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(ossimat C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(ossimat)

pico_generate_pio_header(ossimat ${CMAKE_CURRENT_LIST_DIR}/joybus.pio)
pico_generate_pio_header(ossimat ${CMAKE_CURRENT_LIST_DIR}/psxcontroller.pio)

target_sources(ossimat PRIVATE ossimat.c)

pico_enable_stdio_usb(ossimat 1)
pico_enable_stdio_uart(ossimat 0)

target_link_libraries(ossimat PRIVATE
        pico_stdlib
        hardware_pio
        hardware_spi
        )

pico_add_extra_outputs(ossimat)
