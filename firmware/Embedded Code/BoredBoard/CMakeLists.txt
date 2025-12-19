# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(BoredBoard C CXX ASM)

pico_sdk_init()

# LED Pins have different positions on the Pico v.s. LapisLuzuli (tooling board) 
if(NOT DEFINED ENV{lapisLuzuli})
    set(ENV{lapisLuzuli} "1") # Set the default value only if not set externally
endif()
add_compile_definitions(LAPIS_LUZULI=$ENV{lapisLuzuli})

add_executable(BoredBoard 
            minihdlc/minihdlc.c
            BoredBoard.c )

pico_set_program_name(BoredBoard "BoredBoard")
pico_set_program_version(BoredBoard "0.1")

pico_enable_stdio_usb(BoredBoard 1)
pico_enable_stdio_uart(BoredBoard 0)

target_link_libraries(BoredBoard
        pico_stdlib
        hardware_i2c
        pico_i2c_slave
        hardware_pwm
        pico_multicore
        )

target_include_directories(BoredBoard PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/..
  ${CMAKE_CURRENT_LIST_DIR}/minihdlc
)

target_link_libraries(BoredBoard 
        hardware_i2c
        )

pico_add_extra_outputs(BoredBoard)