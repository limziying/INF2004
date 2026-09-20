cmake_minimum_required(VERSION 3.13)

# Copy pico_sdk_import.cmake from $PICO_SDK_PATH/external/ into this folder
# cd then  Copy-Item "$env:PICO_SDK_PATH\external\pico_sdk_import.cmake" ".\bughunt\"
# cd to bughunt then mkdir build; cd build; cmake -G Ninja -DPICO_BOARD=pico_w ..; ninja
include(pico_sdk_import.cmake)

project(bughunt1 C CXX ASM)

pico_sdk_init()

add_executable(bughunt1
    bughunt1.c
)

target_link_libraries(bughunt1 pico_stdlib)

# Serial output over USB so you can read the test results
pico_enable_stdio_usb(bughunt1 1)
pico_enable_stdio_uart(bughunt1 0)

pico_add_extra_outputs(bughunt1)

# --------------------------------------------------------------------
# The Pico SDK does not enable compiler warnings by default. Several of
# the planted defects in this hunt are reported ONLY as warnings, so the
# briefs would be asking you to read messages that are never emitted.
#
#   -Wextra is needed as well as -Wall: the "unsigned value compared
#   against < 0" defect is -Wtype-limits, which -Wall alone does not
#   include.
#
#   -Wno-unused-parameter suppresses noise from SDK callback signatures
#   that must take arguments they do not use. That noise is not a
#   planted defect.
# --------------------------------------------------------------------
target_compile_options(bughunt1 PRIVATE -Wall -Wextra -Wno-unused-parameter)

