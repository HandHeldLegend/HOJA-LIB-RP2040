add_library(hoja_lib INTERFACE)

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
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/erm_simulator.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/hooks.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/sysmon.c

  ${CMAKE_CURRENT_LIST_DIR}/src/devices/haptics.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/rgb.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/battery.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/fuelgauge.c

  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_commands.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_spi.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_haptics.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_motion.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/input/imu.c 
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
)

if(HOJA_USE_BT_HAL)
  message("HOJA BT DRIVERS IN USE")
  target_link_libraries(hoja_lib INTERFACE
    ${BLUETOOTH_DRIVERS}
  )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/nserial.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)


