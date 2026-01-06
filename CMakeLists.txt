# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
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
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Set paths for Pico Extras and PIO programs
set(PICO_EXTRAS_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-extras")
set(PIO_PATH "${CMAKE_CURRENT_LIST_DIR}/pio")

# Include Pico Extras cmake
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(picoLPTsound C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Include emu8950 library
add_subdirectory(opl)

# Add executable. Default name is the project name, version 0.1

add_executable(picoLPTsound picoLPTsound.c pio_manager.c ringbuffer.c covox.c stereo.c ftl.c dss.c opl2.c tandy.c cms.c square/square.cpp square/square_c.cpp)

# Add all PIOs to compile
pico_generate_pio_header(${PROJECT_NAME}
    ${PIO_PATH}/covox.pio
    ${PIO_PATH}/stereo.pio
    ${PIO_PATH}/ftl.pio
    ${PIO_PATH}/dss.pio
    ${PIO_PATH}/opl2.pio
    ${PIO_PATH}/tandy.pio
    ${PIO_PATH}/cms.pio
)

pico_set_program_name(picoLPTsound "picoLPTsound")
pico_set_program_version(picoLPTsound "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(picoLPTsound 0)
pico_enable_stdio_usb(picoLPTsound 1)

# Add the standard library to the build
target_link_libraries(picoLPTsound
        pico_stdlib
        hardware_pio
        hardware_clocks
        hardware_pwm
        hardware_irq
        pico_audio_i2s 
        pico_multicore
        opl )

# Add the standard include files to the build
target_include_directories(picoLPTsound PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(picoLPTsound)

