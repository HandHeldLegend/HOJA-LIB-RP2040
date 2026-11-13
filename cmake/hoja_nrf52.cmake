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
# The Bluetooth LE controller is its own choice (HOJA_NRF52_BLE, below)
list(FILTER HAL_C EXCLUDE REGEX "/src/hal/nrf52/ble/")

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

# Bluetooth LE: NimBLE's open controller (link layer and nRF radio driver) inside the firmware, with
# no operating system under it (src/hal/nrf52/ble). The board project sets HOJA_NRF52_BLE and gives
# NIMBLE_DIR, an apache/mynewt-nimble checkout.
if(HOJA_NRF52_BLE)
  if(NOT EXISTS "${NIMBLE_DIR}/nimble/controller")
    message(FATAL_ERROR "hoja_nrf52: HOJA_NRF52_BLE needs NIMBLE_DIR, a mynewt-nimble checkout (${NIMBLE_DIR})")
  endif()

  # Our changes to NimBLE (cmake/nimble_patches), on a checkout that does not have them yet. A patch
  # that reverses cleanly is already there.
  find_package(Git REQUIRED)
  file(GLOB _nimble_patches "${HOJA_LIB_ROOT}/cmake/nimble_patches/*.patch")
  foreach(_patch ${_nimble_patches})
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C ${NIMBLE_DIR} apply --reverse --check ${_patch}
      RESULT_VARIABLE _patch_missing OUTPUT_QUIET ERROR_QUIET
    )
    if(_patch_missing)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${NIMBLE_DIR} apply ${_patch}
        RESULT_VARIABLE _patch_failed
      )
      if(_patch_failed)
        message(FATAL_ERROR "hoja_nrf52: ${_patch} does not apply to ${NIMBLE_DIR}")
      endif()
    endif()
  endforeach()

  # NimBLE's own defaults for its settings, as a header (ours come first: nimble/syscfg/syscfg.h)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(_nimble_gen ${CMAKE_CURRENT_BINARY_DIR}/nimble_generated)
  file(MAKE_DIRECTORY ${_nimble_gen})
  execute_process(
    COMMAND ${Python3_EXECUTABLE} ${HOJA_LIB_ROOT}/cmake/nimble_syscfg.py ${NIMBLE_DIR}
            ${_nimble_gen}/nimble_syscfg_defaults.h
    RESULT_VARIABLE _nimble_gen_result
  )
  if(_nimble_gen_result)
    message(FATAL_ERROR "hoja_nrf52: could not generate NimBLE's settings header (PyYAML installed?)")
  endif()

  file(GLOB NIMBLE_CONTROLLER_C "${NIMBLE_DIR}/nimble/controller/src/*.c")
  # Channel sounding (off) is the only part that needs a crypto library
  list(FILTER NIMBLE_CONTROLLER_C EXCLUDE REGEX "/ble_ll_(cs|cs_drbg|crypto)\.c$")
  file(GLOB HOJA_NRF52_BLE_C "${HOJA_LIB_ROOT}/src/hal/nrf52/ble/*.c")
  target_sources(hoja_lib INTERFACE
    ${HOJA_NRF52_BLE_C}
    ${NIMBLE_CONTROLLER_C}
    ${NIMBLE_DIR}/nimble/drivers/nrf5x/src/ble_hw.c
    ${NIMBLE_DIR}/nimble/drivers/nrf5x/src/ble_phy.c
    ${NIMBLE_DIR}/nimble/drivers/nrf5x/src/nrf52/phy.c
    ${NIMBLE_DIR}/nimble/transport/src/transport.c
    ${NIMBLE_DIR}/porting/nimble/src/endian.c
    ${NIMBLE_DIR}/porting/nimble/src/mem.c
    ${NIMBLE_DIR}/porting/nimble/src/os_mbuf.c
    ${NIMBLE_DIR}/porting/nimble/src/os_mempool.c
    ${NIMBLE_DIR}/porting/nimble/src/os_msys.c
    ${NIMBLE_DIR}/porting/nimble/src/os_cputime.c
    ${NIMBLE_DIR}/porting/nimble/src/os_cputime_pwr2.c
    ${NIMBLE_DIR}/porting/nimble/src/hal_timer.c
  )
  target_include_directories(hoja_lib INTERFACE
    # Ours first: its syscfg/syscfg.h stands in front of the one NimBLE's porting folder ships
    ${HOJA_LIB_ROOT}/include/hal/nrf52/nimble
    ${_nimble_gen}
    ${NIMBLE_DIR}/porting/nimble/include
    ${NIMBLE_DIR}/nimble/include
    ${NIMBLE_DIR}/nimble/controller/include
    # Private to the controller: the port sets what its task sets before its loop
    ${NIMBLE_DIR}/nimble/controller/src
    ${NIMBLE_DIR}/nimble/drivers/nrf5x/include
    ${NIMBLE_DIR}/nimble/drivers/nrf5x/src
    ${NIMBLE_DIR}/nimble/transport/include
  )
  target_compile_definitions(hoja_lib INTERFACE
    HOJA_NRF52_BLE=1
    NIMBLE_CFG_CONTROLLER=1
  )
endif()

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
