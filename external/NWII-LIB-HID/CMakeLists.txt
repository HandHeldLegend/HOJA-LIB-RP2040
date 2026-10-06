# NWII-LIB-HID — portable Wii Remote (+ Nunchuk / Classic Controller) emulation over Bluetooth HID.
#
# Free and unencumbered software released into the public domain (The Unlicense). See LICENSE.
# Written with the help of Claude Opus (Anthropic).
#
# SPDX-License-Identifier: Unlicense
#
# Usage from a parent CMake project (after add_subdirectory):
#   target_link_libraries(<your_firmware> PRIVATE nwii_lib_hid)
#
# Public headers live in include/; compile this target as C11 or newer.

add_library(nwii_lib_hid STATIC
  nwii_lib_crypto.c
  nwii_lib_extension.c
  nwii_lib_ir.c
  nwii_lib_hid.c
  nwii_lib_protocol.c
  nwii_lib.c
)

target_include_directories(nwii_lib_hid PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_compile_features(nwii_lib_hid PUBLIC c_std_11)

# Weak callback stubs in nwii_lib.c are meant to be overridden by the firmware / platform layer.
