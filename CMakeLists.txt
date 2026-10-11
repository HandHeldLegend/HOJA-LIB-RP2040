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
  #CYW43_LWIP=0
  LWIP_DEBUG=0
  LWIP_STATS=0
  LWIP_STATS_DISPLAY=0
  # Float helpers run on every input cycle: keep them out of the flash cache's way
  PICO_FLOAT_IN_RAM=1
)

set(BLUETOOTH_DRIVERS 
    pico_cyw43_arch_lwip_threadsafe_background 
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

add_library(hoja_bt_floss INTERFACE)
target_sources(hoja_bt_floss INTERFACE
  ${FLOSS_C}
  ${FLOSS_ROOT}/transport/cyw43/floss_cyw43.c
)
target_include_directories(hoja_bt_floss INTERFACE
  ${FLOSS_ROOT}/include
  ${FLOSS_ROOT}/src
  ${FLOSS_ROOT}/transport/cyw43
  # cyw43_arch includes this when the radio firmware is enabled; the shared-bus header is for
  # FLOSS's fast bus path
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/cybt_shared_bus
)
target_compile_definitions(hoja_bt_floss INTERFACE
  HOJA_BT_FLOSS=1
  FLOSS_USER_CONFIG_FILE="floss_config_hoja.h"
  CYW43_ENABLE_BLUETOOTH=1 # Combined WLAN + radio firmware
)
target_link_libraries(hoja_bt_floss INTERFACE
  pico_cyw43_arch_lwip_threadsafe_background
  pico_cyw43_driver
)

add_library(hoja_bt_btstack INTERFACE)
target_link_libraries(hoja_bt_btstack INTERFACE
  ${BLUETOOTH_DRIVERS}
)

# BTstack unless the board sets the HOJA_BT_FLOSS target property (its board.cmake)
if(HOJA_USE_BT_HAL)
  message("HOJA BT DRIVERS IN USE")
  target_link_libraries(hoja_lib INTERFACE
    $<IF:$<BOOL:$<TARGET_PROPERTY:HOJA_BT_FLOSS>>,hoja_bt_floss,hoja_bt_btstack>
  )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/nesbus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)


