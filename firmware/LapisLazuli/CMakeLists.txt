cmake_minimum_required(VERSION 3.13)

include("${CMAKE_CURRENT_LIST_DIR}/pico-sdk/pico_sdk_init.cmake")

project(LapisLazuli C CXX ASM)

pico_sdk_init()

# LED Pins have different positions on the Pico v.s. LapisLazuli (tooling board) 
if(NOT DEFINED ENV{LapisLazuli})
    set(ENV{LapisLazuli} "1") # Set the default value only if not set externally
endif()
add_compile_definitions(LAPIS_LAZULI=$ENV{LapisLazuli})

add_executable(LapisLazuli 
            minihdlc/minihdlc.c
            main.c )

pico_set_program_name(LapisLazuli "LapisLazuli")
pico_set_program_version(LapisLazuli "0.1")

pico_enable_stdio_usb(LapisLazuli 1)
pico_enable_stdio_uart(LapisLazuli 0)

target_link_libraries(LapisLazuli
        pico_stdlib
        hardware_i2c
        pico_i2c_slave
        hardware_pwm
        pico_multicore
        )

target_include_directories(LapisLazuli PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
  ${CMAKE_CURRENT_LIST_DIR}/minihdlc
)

target_link_libraries(LapisLazuli 
        hardware_i2c
        )

pico_add_extra_outputs(LapisLazuli)