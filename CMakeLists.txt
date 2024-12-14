add_library(hoja_lib INTERFACE)

file(GLOB_RECURSE DRIVERS_C "${CMAKE_CURRENT_LIST_DIR}/src/drivers/*.c")
file(GLOB_RECURSE HAL_C "${CMAKE_CURRENT_LIST_DIR}/src/hal/rp2040/*.c")
file(GLOB_RECURSE RGB_C "${CMAKE_CURRENT_LIST_DIR}/src/devices/animations/*.c")

message("Globbed C files: ${DRIVERS_C}")
message("Globbed HAL files: ${HAL_C}")

# Pull in pico libraries that we need
target_link_libraries(hoja_lib INTERFACE
  pico_stdlib
  pico_bootrom
  pico_multicore
  pico_platform
  pico_rand
  pico_float
  pico_flash

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

target_sources(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja.c
  
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/reboot.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/interval.c
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/ringbuffer.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/settings.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/static_config.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/devices/devices.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/devices/haptics.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/rgb.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/battery.c
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/bluetooth.c 

  
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_commands.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_spi.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_haptics.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_motion.c 


  ${CMAKE_CURRENT_LIST_DIR}/src/input/input.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/input/button.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/imu.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/remap.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/snapback.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/stick_scaling.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/triggers.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/macros.c 


  ${CMAKE_CURRENT_LIST_DIR}/src/usb/usb.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/usb/desc_bos.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/xinput_usbd.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/ginput_usbd.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/usb/swpro.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/xinput.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/ds4input.c
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/dinput.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/gcinput.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/usb/webusb.c 
  
  ${CMAKE_CURRENT_LIST_DIR}/src/wired/wired.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/wired/gamecube.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/wired/n64.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/wired/nspi.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/hal/hal.c

  ${DRIVERS_C}
  ${HAL_C}
  ${RGB_C}
)

target_include_directories(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/generated 
  ${CMAKE_CURRENT_LIST_DIR}/include/hal/rp2040
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/nserial.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)


