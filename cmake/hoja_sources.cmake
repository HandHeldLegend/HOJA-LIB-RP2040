# The library's sources shared by every chip family. HOJA_LIB_ROOT is the library's folder; the
# family's own file list (its HAL under src/hal/<family>) is added by whoever includes this.
file(GLOB_RECURSE DRIVERS_C "${HOJA_LIB_ROOT}/src/drivers/*.c")
file(GLOB_RECURSE RGB_C "${HOJA_LIB_ROOT}/src/devices/animations/*.c")
file(GLOB_RECURSE MACROS_C "${HOJA_LIB_ROOT}/src/input/macros/*.c")
file(GLOB_RECURSE SNAPBACK_C "${HOJA_LIB_ROOT}/src/input/snapback/*.c")
file(GLOB_RECURSE CORES_C "${HOJA_LIB_ROOT}/src/cores/*.c")
file(GLOB_RECURSE TRANSPORTS_C "${HOJA_LIB_ROOT}/src/transport/*.c")

set(HOJA_COMMON_SOURCES
  ${HOJA_LIB_ROOT}/src/hoja.c
  ${HOJA_LIB_ROOT}/src/utilities/boot.c
  ${HOJA_LIB_ROOT}/src/utilities/interval.c
  ${HOJA_LIB_ROOT}/src/utilities/ringbuffer.c
  ${HOJA_LIB_ROOT}/src/utilities/settings.c
  ${HOJA_LIB_ROOT}/src/utilities/profiles.c
  ${HOJA_LIB_ROOT}/src/utilities/static_config.c
  ${HOJA_LIB_ROOT}/src/utilities/pcm.c
  ${HOJA_LIB_ROOT}/src/utilities/hooks.c
  ${HOJA_LIB_ROOT}/src/utilities/sysmon.c
  ${HOJA_LIB_ROOT}/src/utilities/tasks.c
  ${HOJA_LIB_ROOT}/src/devices/haptics.c
  ${HOJA_LIB_ROOT}/src/devices/rgb.c
  ${HOJA_LIB_ROOT}/src/devices/battery.c
  ${HOJA_LIB_ROOT}/src/devices/fuelgauge.c
  ${HOJA_LIB_ROOT}/src/input/imu.c
  ${HOJA_LIB_ROOT}/src/input/motion_gesture.c
  ${HOJA_LIB_ROOT}/src/utilities/autodetect.c
  ${HOJA_LIB_ROOT}/src/utilities/boot_timeline.c
  ${HOJA_LIB_ROOT}/src/utilities/ns2_bt_probe.c
  ${HOJA_LIB_ROOT}/src/input/mapper.c
  ${HOJA_LIB_ROOT}/src/input/snapback.c
  ${HOJA_LIB_ROOT}/src/input/stick_scaling.c
  ${HOJA_LIB_ROOT}/src/input/stick_deadzone.c
  ${HOJA_LIB_ROOT}/src/input/analog.c
  ${HOJA_LIB_ROOT}/src/input/dpad.c
  ${HOJA_LIB_ROOT}/src/input/macros.c
  ${HOJA_LIB_ROOT}/src/input/idle_manager.c
  ${HOJA_LIB_ROOT}/src/input/hover.c
  ${HOJA_LIB_ROOT}/src/usb/webusb.c

  ${MACROS_C}
  ${DRIVERS_C}
  ${RGB_C}
  ${SNAPBACK_C}
  ${CORES_C}
  ${TRANSPORTS_C}
)
