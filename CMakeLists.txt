add_library(hoja_lib INTERFACE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/NS-LIB-HID")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/NWII-LIB-HID")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/SINPUT-LIB-HID")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/HOJA-LIB-DONGLE")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/HHL-TINYUSB-DRIVERS")

file(GLOB_RECURSE DRIVERS_C "${CMAKE_CURRENT_LIST_DIR}/src/drivers/*.c")
file(GLOB_RECURSE HAL_C "${CMAKE_CURRENT_LIST_DIR}/src/hal/rp2040/*.c")
file(GLOB_RECURSE RGB_C "${CMAKE_CURRENT_LIST_DIR}/src/devices/animations/*.c")
file(GLOB_RECURSE MACROS_C "${CMAKE_CURRENT_LIST_DIR}/src/input/macros/*.c")
file(GLOB_RECURSE SNAPBACK_C "${CMAKE_CURRENT_LIST_DIR}/src/input/snapback/*.c")
file(GLOB_RECURSE CORES_C "${CMAKE_CURRENT_LIST_DIR}/src/cores/*.c")
file(GLOB_RECURSE TRANSPORTS_C "${CMAKE_CURRENT_LIST_DIR}/src/transport/*.c")

message("Globbed C files: ${DRIVERS_C}")
message("Globbed HAL files: ${HAL_C}")

target_sources(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja.c
  
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/boot.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/interval.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/ringbuffer.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/settings.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/static_config.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/pcm.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/hooks.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/sysmon.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/tasks.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/devices/haptics.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/rgb.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/battery.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/fuelgauge.c

  ${CMAKE_CURRENT_LIST_DIR}/src/input/imu.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/motion_gesture.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/autodetect.c
  ${CMAKE_CURRENT_LIST_DIR}/src/input/mapper.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/snapback.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/stick_scaling.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/stick_deadzone.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/dpad.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/macros.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/idle_manager.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/hover.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/usb/webusb.c 

  ${MACROS_C} 
  ${DRIVERS_C}
  ${HAL_C}
  ${RGB_C}
  ${SNAPBACK_C}
  ${CORES_C}
  ${TRANSPORTS_C}
)

target_include_directories(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/generated 
  ${CMAKE_CURRENT_LIST_DIR}/include/hal/rp2040
)

target_compile_definitions(hoja_lib
  INTERFACE
  # Float helpers run on every input cycle: keep them out of the flash cache's way
  PICO_FLOAT_IN_RAM=1
)

set(BLUETOOTH_DRIVERS 
    pico_cyw43_arch_none 
    pico_cyw43_driver 
    pico_btstack_cyw43 
    pico_btstack_hci_transport_cyw43 
    pico_btstack_flash_bank
    pico_btstack_ble 
    pico_btstack_classic 
)


# Pull in pico libraries that we need
target_link_libraries(hoja_lib INTERFACE
  pico_stdlib 
  pico_bootrom 
  pico_multicore 
  pico_flash 
  pico_platform
  pico_rand
  pico_float

  hardware_exception
  hardware_gpio
  hardware_adc
  hardware_flash
  hardware_pio
  hardware_dma
  hardware_pwm
  hardware_spi
  hardware_sync
  hardware_timer
  hardware_irq
  hardware_i2c

  tinyusb_device
  tinyusb_board

  ns_lib_hid
  nwii_lib_hid
  sinput_lib_hid
  hoja_lib_dongle
  hhl_tinyusb_drivers
)

# FLOSS as the Bluetooth host stack, for boards that opt in (see below)
set(FLOSS_ROOT ${CMAKE_CURRENT_LIST_DIR}/external/floss)
file(GLOB_RECURSE FLOSS_C "${FLOSS_ROOT}/src/*.c")

# The stack alone, for a radio whose driver is part of HOJA (the ESP32 HCI bridge, esp32_hci.c)
add_library(hoja_bt_floss_core INTERFACE)
target_sources(hoja_bt_floss_core INTERFACE
  ${FLOSS_C}
)
target_include_directories(hoja_bt_floss_core INTERFACE
  ${FLOSS_ROOT}/include
  ${FLOSS_ROOT}/src
)
target_compile_definitions(hoja_bt_floss_core INTERFACE
  HOJA_BT_FLOSS=1
  FLOSS_USER_CONFIG_FILE="floss_config_hoja.h"
)

# The stack with FLOSS's CYW43 transport, for the RM2
add_library(hoja_bt_floss INTERFACE)
target_sources(hoja_bt_floss INTERFACE
  ${FLOSS_ROOT}/transport/cyw43/floss_cyw43.c
)
target_include_directories(hoja_bt_floss INTERFACE
  ${FLOSS_ROOT}/transport/cyw43
  # cyw43_arch includes this when the radio firmware is enabled; the shared-bus header is for
  # FLOSS's fast bus path
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/cybt_shared_bus
)
target_compile_definitions(hoja_bt_floss INTERFACE
  CYW43_ENABLE_BLUETOOTH=1 # Combined WLAN + radio firmware
)
target_link_libraries(hoja_bt_floss INTERFACE
  hoja_bt_floss_core
  pico_cyw43_arch_none
  pico_cyw43_driver
)

add_library(hoja_bt_btstack INTERFACE)
target_link_libraries(hoja_bt_btstack INTERFACE
  ${BLUETOOTH_DRIVERS}
)

# A board's board.cmake calls this to use FLOSS. The code on every report's path (the radio's bus
# driver, FLOSS's poll, HCI, L2CAP data and HID send) runs from RAM: from flash, the rest of the
# task loop evicts it from the flash cache between reports and every exchange with the radio runs
# several times slower. Pairing, service discovery and connection setup stay in flash.
function(hoja_use_floss TARGET)
  set_target_properties(${TARGET} PROPERTIES HOJA_BT_FLOSS ON)

  set(_ram_objects
    */floss/src/floss.c.obj
    */floss/src/core/*.c.obj
    *floss_hci.c.obj
    *floss_hci_acl.c.obj
    *floss_l2cap.c.obj
    *floss_hid.c.obj
    *floss_link.c.obj
    *floss_link_classic.c.obj
    *floss_cyw43.c.obj
    *cyw43_ll.c.obj
    *cyw43_ctrl.c.obj
    *cyw43_bus_pio_spi.c.obj
    *cybt_shared_bus_driver.c.obj
    *esp32_hci.c.obj
    *hlink.c.obj
  )

  set(_ld_in ${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2040/memmap_default.ld)
  set(_ld_out ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_floss_ram.ld)
  file(READ ${_ld_in} _ld)
  set(_flash_text "*(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a:) .text*)")
  string(FIND "${_ld}" "${_flash_text}" _at)
  if(_at EQUAL -1)
    message(FATAL_ERROR "hoja_use_floss: unexpected linker script ${_ld_in}")
  endif()
  list(JOIN _ram_objects " " _ram_objects)
  string(REPLACE "${_flash_text}"
    "*(EXCLUDE_FILE(*libgcc.a: *libc.a:*lib_a-mem*.o *libm.a: ${_ram_objects}) .text*)"
    _ld "${_ld}")
  file(WRITE ${_ld_out} "${_ld}")
  pico_set_linker_script(${TARGET} ${_ld_out})
endfunction()

# The HOJA bootloader in front of the firmware (include/hal/rp2040/boot_layout.h): the board's
# board.cmake calls hoja_use_bootloader(${PROJECT_NAME} "<device name>" <flash bytes> <radio bytes>),
# after hoja_use_floss. Radio bytes: the CYW43 firmware's fixed region (0 for boards without one).
# The bootloader builds as <board>_boot; its binary and the radio region go into the firmware's ELF,
# so the board's .bin and .uf2 hold all of it, written as one.
function(hoja_use_bootloader TARGET DEVICE_NAME FLASH_BYTES RADIO_BYTES)
  set(HOJA_LIB_DIR_ABS ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
  # These match BOOT_LAYOUT_BOOT_SIZE and BOOT_LAYOUT_TOP_SIZE in boot_layout.h
  set(HOJA_BOOT_SIZE 0x8000)
  set(HOJA_BOOT_TOP_SIZE 0x10000)

  math(EXPR _flash "${FLASH_BYTES}")
  math(EXPR _radio "${RADIO_BYTES}")
  # The firmware may fill everything up to the stage header (BOOT_LAYOUT_STAGE_HEADER_OFFSET)
  math(EXPR _flash_region "${_flash} - ${HOJA_BOOT_TOP_SIZE} - 4096")

  # The firmware: the bootloader's binary at the start of flash, the radio region after it, then
  # the firmware. board.cmake runs twice, and hoja_use_floss sets its script again, so this patches
  # whichever script the board has now.
  get_target_property(_ld_in ${TARGET} PICO_TARGET_LINKER_SCRIPT)
  if(NOT _ld_in)
    set(_ld_in ${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2040/memmap_default.ld)
  endif()
  set(_ld_out ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_app_slot.ld)
  if(NOT _ld_in STREQUAL _ld_out)
    file(READ ${_ld_in} _ld)
    set(_region "INCLUDE \"pico_flash_region.ld\"")
    set(_begin "    .flash_begin : {")
    string(FIND "${_ld}" "${_region}" _at_region)
    string(FIND "${_ld}" "${_begin}" _at_begin)
    if(_at_region EQUAL -1 OR _at_begin EQUAL -1)
      message(FATAL_ERROR "hoja_use_bootloader: unexpected linker script ${_ld_in}")
    endif()
    string(REPLACE "${_region}" "FLASH(rx) : ORIGIN = 0x10000000, LENGTH = ${_flash_region}" _ld "${_ld}")

    string(CONCAT _front
      "    .hoja_bootloader : {\n"
      "        KEEP (*(.hoja_bootloader))\n"
      "        . = ${HOJA_BOOT_SIZE};\n"
      "    } > FLASH = 0xFF\n\n")
    if(_radio GREATER 0)
      # The CYW43 firmware at fixed addresses, behind its header (src/hal/rp2040/boot_radio.c)
      string(APPEND _front
        "    .hoja_radio : {\n"
        "        KEEP (*(.hoja_radio_header))\n"
        "        . = ALIGN(4);\n"
        "        __hoja_radio_wifi_start = .;\n"
        "        KEEP (*(.rodata.wb43439A0_*_combined))\n"
        "        __hoja_radio_wifi_end = .;\n"
        "        . = ALIGN(4);\n"
        "        __hoja_radio_bt_start = .;\n"
        "        KEEP (*(.rodata.cyw43_btfw_43439))\n"
        "        __hoja_radio_bt_end = .;\n"
        "        . = ${_radio};\n"
        "    } > FLASH = 0xFF\n"
        "    ASSERT(__hoja_radio_wifi_end > __hoja_radio_wifi_start && __hoja_radio_bt_end > __hoja_radio_bt_start,\n"
        "        \"hoja_use_bootloader: the CYW43 firmware was not found for the radio region\")\n\n")
    endif()
    string(REPLACE "${_begin}" "${_front}${_begin}" _ld "${_ld}")
    file(WRITE ${_ld_out} "${_ld}")
    pico_set_linker_script(${TARGET} ${_ld_out})
  endif()

  get_target_property(_done ${TARGET} HOJA_BOOTLOADER)
  if(_done)
    return()
  endif()
  set_target_properties(${TARGET} PROPERTIES HOJA_BOOTLOADER ON)
  target_compile_definitions(${TARGET} PRIVATE
    HOJA_BOOTLOADER=1
    HOJA_BOOT_FLASH_SIZE_BYTES=${_flash}
    HOJA_BOOT_RADIO_SIZE=${_radio}
  )

  set(_boot ${TARGET}_boot)
  add_executable(${_boot}
    ${HOJA_LIB_DIR_ABS}/bootloader/boot_main.c
    ${HOJA_LIB_DIR_ABS}/bootloader/boot_flash.c
    ${HOJA_LIB_DIR_ABS}/bootloader/boot_stage.c
    ${HOJA_LIB_DIR_ABS}/bootloader/boot_usb.c
  )
  target_include_directories(${_boot} PRIVATE
    ${HOJA_LIB_DIR_ABS}/bootloader
    ${HOJA_LIB_DIR_ABS}/include
  )
  target_compile_definitions(${_boot} PRIVATE
    BOOT_DEVICE_NAME="${DEVICE_NAME}"
    BOOT_BUILD_ID="${TARGET}"
    PICO_FLASH_SIZE_BYTES=${_flash}
    HOJA_BOOT_FLASH_SIZE_BYTES=${_flash}
    HOJA_BOOT_RADIO_SIZE=${_radio}
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1
    PICO_PANIC_FUNCTION=boot_panic
  )
  target_link_libraries(${_boot} PRIVATE pico_stdlib hardware_flash tinyusb_device)
  pico_enable_stdio_usb(${_boot} 0)
  pico_enable_stdio_uart(${_boot} 0)
  # It prints nothing and counts in integers: none of their code in its 32 KB. TinyUSB logs through
  # printf in Debug builds (CFG_TUSB_DEBUG=1 for every user), which would panic here: none of that
  # either (flags follow the SDK's definitions, so these win).
  target_compile_options(${_boot} PRIVATE -UCFG_TUSB_DEBUG -DCFG_TUSB_DEBUG=0)
  pico_set_printf_implementation(${_boot} none)
  pico_set_float_implementation(${_boot} none)
  pico_set_double_implementation(${_boot} none)
  pico_set_program_name(${_boot} "${TARGET}")

  # The bootloader has its own sectors and no more
  file(READ ${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2040/memmap_default.ld _boot_ld)
  string(REPLACE "INCLUDE \"pico_flash_region.ld\"" "FLASH(rx) : ORIGIN = 0x10000000, LENGTH = ${HOJA_BOOT_SIZE}"
    _boot_ld "${_boot_ld}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_boot}.ld "${_boot_ld}")
  pico_set_linker_script(${_boot} ${CMAKE_CURRENT_BINARY_DIR}/${_boot}.ld)

  set_target_properties(${_boot} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  pico_add_bin_output(${_boot})
  pico_add_dis_output(${_boot})

  # Into the firmware's ELF, assembled again whenever the bootloader changes
  set(_boot_bin ${CMAKE_CURRENT_BINARY_DIR}/${_boot}.bin)
  set(_boot_s ${CMAKE_CURRENT_BINARY_DIR}/${_boot}_image.S)
  file(WRITE ${_boot_s} ".section .hoja_bootloader, \"a\"\n.incbin \"${_boot_bin}\"\n")
  set_source_files_properties(${_boot_s} PROPERTIES OBJECT_DEPENDS ${_boot_bin})
  target_sources(${TARGET} PRIVATE ${_boot_s})
  add_dependencies(${TARGET} ${_boot})
endfunction()

# BTstack unless the board sets the HOJA_BT_FLOSS target property (hoja_use_floss)
if(HOJA_USE_BT_HAL)
  message("HOJA BT DRIVERS IN USE")
  target_link_libraries(hoja_lib INTERFACE
    $<IF:$<BOOL:$<TARGET_PROPERTY:HOJA_BT_FLOSS>>,hoja_bt_floss,hoja_bt_btstack>
  )
else()
  # Boards without the RM2 (the ESP32 HCI bridge) link their own stack, or FLOSS through this
  target_link_libraries(hoja_lib INTERFACE
    $<$<BOOL:$<TARGET_PROPERTY:HOJA_BT_FLOSS>>:hoja_bt_floss_core>
  )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/nesbus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)


