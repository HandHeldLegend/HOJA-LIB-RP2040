# HOJA on Nordic's nRF52 family (nRF52833): the shared library sources over src/hal/nrf52, built
# against nrfx's register headers (no nrfx drivers) and TinyUSB's nRF5x device driver.
#
# The board project sets, before adding the library folder:
#   HOJA_PLATFORM  nrf52
#   NRFX_DIR       an nrfx checkout (mdk/, hal/, soc/)
#   CMSIS_DIR      a CMSIS checkout (CMSIS/Core/Include)
#   TINYUSB_DIR    a TinyUSB checkout (src/, hw/bsp/nrf)
#   HOJA_NRF52_MCU the MDK's name for the chip, such as NRF52833_XXAA

foreach(_dir NRFX_DIR CMSIS_DIR TINYUSB_DIR)
  if(NOT EXISTS "${${_dir}}")
    message(FATAL_ERROR "hoja_nrf52: ${_dir} is not set to a checkout (${${_dir}})")
  endif()
endforeach()
if(NOT DEFINED HOJA_NRF52_MCU)
  set(HOJA_NRF52_MCU NRF52833_XXAA)
endif()
string(TOLOWER "${HOJA_NRF52_MCU}" _mcu_lower)
string(REGEX REPLACE "_xx..$" "" HOJA_NRF52_CHIP "${_mcu_lower}") # nrf52833

set(HOJA_LIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
add_library(hoja_lib INTERFACE)

add_subdirectory("${HOJA_LIB_ROOT}/external/NS-LIB-HID")
add_subdirectory("${HOJA_LIB_ROOT}/external/NWII-LIB-HID")
add_subdirectory("${HOJA_LIB_ROOT}/external/SINPUT-LIB-HID")
add_subdirectory("${HOJA_LIB_ROOT}/external/HOJA-LIB-DONGLE")
add_subdirectory("${HOJA_LIB_ROOT}/external/HHL-TINYUSB-DRIVERS")

set(HOJA_NS2_LIB_DIR "${HOJA_LIB_ROOT}/external/NS2-LIB-HID" CACHE PATH "NS2-LIB-HID checkout")
set(HOJA_NS2_USB_PAIRING "1" CACHE STRING "How the Switch 2's pairing is answered (ns2_usb_pairing_t; 1 declines: no slot)")
if(EXISTS "${HOJA_NS2_LIB_DIR}/CMakeLists.txt")
  add_subdirectory("${HOJA_NS2_LIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ns2_lib_hid")
  target_link_libraries(hoja_lib INTERFACE ns2_lib_hid)
  target_compile_definitions(hoja_lib INTERFACE HOJA_NS2=1 HOJA_NS2_USB_PAIRING=${HOJA_NS2_USB_PAIRING})
endif()

include(${HOJA_LIB_ROOT}/cmake/hoja_sources.cmake)
file(GLOB_RECURSE HAL_C "${HOJA_LIB_ROOT}/src/hal/nrf52/*.c")

# TinyUSB as a device, with the classes HHL-TINYUSB-DRIVERS builds on
set(TINYUSB_C
  ${TINYUSB_DIR}/src/tusb.c
  ${TINYUSB_DIR}/src/common/tusb_fifo.c
  ${TINYUSB_DIR}/src/device/usbd.c
  ${TINYUSB_DIR}/src/device/usbd_control.c
  ${TINYUSB_DIR}/src/class/hid/hid_device.c
  ${TINYUSB_DIR}/src/class/vendor/vendor_device.c
  ${TINYUSB_DIR}/src/class/cdc/cdc_device.c
  ${TINYUSB_DIR}/src/portable/nordic/nrf5x/dcd_nrf5x.c
)

target_sources(hoja_lib INTERFACE
  ${HOJA_COMMON_SOURCES}
  ${HAL_C}
  ${TINYUSB_C}
  # The vector table and reset code (ours: it takes the core back from a bootloader), and the chip's
  # clock/errata setup
  ${HOJA_LIB_ROOT}/src/hal/nrf52/startup_${HOJA_NRF52_CHIP}.S
  ${NRFX_DIR}/mdk/system_${HOJA_NRF52_CHIP}.c
)

target_include_directories(hoja_lib INTERFACE
  ${HOJA_LIB_ROOT}/include
  ${HOJA_LIB_ROOT}/include/hal/nrf52
  ${TINYUSB_DIR}/src
  # nrfx's glue and config as TinyUSB's own nRF boards use them
  ${TINYUSB_DIR}/hw/bsp/nrf
  ${NRFX_DIR}
  ${NRFX_DIR}/mdk
  ${NRFX_DIR}/hal
  ${NRFX_DIR}/drivers/include
  ${NRFX_DIR}/drivers/src
  ${CMSIS_DIR}/CMSIS/Core/Include
)

target_compile_definitions(hoja_lib INTERFACE
  ${HOJA_NRF52_MCU}
  CFG_TUSB_MCU=OPT_MCU_NRF5X
  # The reset code enters the HAL, which hands on to the C library
  __START=sys_hal_nrf52_start
  __STACK_SIZE=8192
  __HEAP_SIZE=2048
)

target_link_libraries(hoja_lib INTERFACE
  ns_lib_hid
  nwii_lib_hid
  sinput_lib_hid
  hoja_lib_dongle
  hhl_tinyusb_drivers
  m
)

# A board's firmware: the linker script with the board's flash and RAM (MEMORY, then nrfx's
# nrf_common.ld), and a raw .bin beside the .elf.
function(hoja_nrf52_firmware TARGET LINKER_SCRIPT)
  target_link_libraries(${TARGET} PRIVATE hoja_lib)
  target_link_options(${TARGET} PRIVATE
    -T${LINKER_SCRIPT}
    -L${NRFX_DIR}/mdk
    -Wl,--gc-sections
    # usb_hal.c stands in front of TinyUSB's frame-interrupt switch (see __wrap_dcd_sof_enable)
    -Wl,--wrap=dcd_sof_enable
    -Wl,--print-memory-usage
    -Wl,-Map=$<TARGET_FILE_DIR:${TARGET}>/${TARGET}.map
    --specs=nano.specs
    --specs=nosys.specs
  )
  set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LINKER_SCRIPT})
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
    COMMENT "Writing ${TARGET}.bin"
  )
endfunction()
