# NS-LIB-HID — portable console-oriented HID/core helper for emulating a Nintendo Switch gamepad.
#
# Licensing: see LICENSING.md for per-file terms.
#
# Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
#
# Build metadata in this file: CC BY-NC 4.0 (see LICENSING.md). SPDX-License-Identifier: CC-BY-NC-4.0
#
# Usage from a parent CMake project (after add_subdirectory):
#   target_link_libraries(<your_firmware> PRIVATE NS-LIB-HID)
#
# Public headers live in include/; compile this target as C11 or newer.

add_library(ns_lib_hid STATIC
  ns_lib_config.c
  ns_lib_analog.c
  ns_lib_haptics.c
  ns_lib_motion.c
  ns_lib_spi.c
  ns_lib_protocol.c
  ns_lib.c
)

target_include_directories(ns_lib_hid PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_compile_features(ns_lib_hid PUBLIC c_std_11)

if(NOT MSVC)
  target_link_libraries(ns_lib_hid PRIVATE m)
endif()

# Weak callback stubs in ns_lib.c are meant to be overridden by the firmware / platform layer.
