add_library(hoja_lib INTERFACE)

target_sources(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja.c
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja_settings.c
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja_defines.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/hoja_comms.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/utilities/rgb.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/usb/desc_bos.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/hoja_usb.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/usb/xinput_device.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_commands.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/switch/switch_spi.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/input/imu.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/remap.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/snapback.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/stick_scaling.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/analog.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/input/safemode.c 

  ${CMAKE_CURRENT_LIST_DIR}/src/devices/dinput.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/gcinput.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/gamecube.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/n64.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/nspi.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/swpro.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/webusb.c 
  ${CMAKE_CURRENT_LIST_DIR}/src/devices/xinput.c 
)

target_include_directories(hoja_lib INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/devices 
  ${CMAKE_CURRENT_LIST_DIR}/include/generated 
  ${CMAKE_CURRENT_LIST_DIR}/include/input 
  ${CMAKE_CURRENT_LIST_DIR}/include/switch 
  ${CMAKE_CURRENT_LIST_DIR}/include/usb 
  ${CMAKE_CURRENT_LIST_DIR}/include/utilities
)

#pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio)
#pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/../pio/cvideo.pio)

#add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/include/generated/joybus.pio.h
#	DEPENDS ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio
#	COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio ${CMAKE_CURRENT_LIST_DIR}/include/generated/joybus.pio.h
#)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/nserial.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
pico_generate_pio_header(hoja_lib ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)


#add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.pio.h
#	DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio
#	COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/cvideo.pio ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo.pio.h
#	)

# Pull in pico libraries that we need
target_link_libraries(hoja_lib INTERFACE
  pico_stdlib
  pico_bootrom
  pico_multicore
  pico_platform
  pico_rand

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

  tinyusb_device
  tinyusb_board
)

