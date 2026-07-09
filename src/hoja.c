#include "hoja.h"
#include "usb/webusb.h"

#include "hal/sys_hal.h"
#include "hal/flash_hal.h"
#include "hal/i2c_hal.h"
#include "hal/spi_hal.h"

#include "board_config.h"

#include "cores/cores.h"
#include "transport/transport.h"
#include "transport/transport_usb.h"

#include "utilities/callback.h"
#include "utilities/settings.h"
#include "utilities/boot.h"
#include "utilities/hooks.h"
#include "utilities/sysmon.h"

#include "input/analog.h"
#include "input/imu.h"
#include "input/macros.h"
#include "input/idle_manager.h"
#include "input/mapper.h"
#include "input/hover.h"
#include "input/stick_scaling.h"

#include "hardware/sync.h"
#include "devices/fuelgauge.h"
#include "utilities/tasks.h"

#include "devices/battery.h"
#include "devices/rgb.h"

#include "devices/haptics.h"
#include "devices/fuelgauge.h"

static const hoja_config_s *_hoja_config = NULL;

const hoja_config_s *hoja_config_get(void)
{
  return _hoja_config;
}

static void _i2c_hal_init_one(uint8_t hw_instance, const hoja_i2c_instance_cfg_s *inst)
{
  if(!inst || !inst->enabled)
    return;
  uint32_t baud_khz = inst->baudrate_khz ? inst->baudrate_khz : HOJA_I2C_DEFAULT_BAUDRATE_KHZ;
  i2c_hal_init(hw_instance, inst->sda_gpio, inst->scl_gpio, baud_khz);
}

static void _spi_hal_init_one(uint8_t hw_instance, const hoja_spi_instance_cfg_s *inst)
{
  if(!inst || !inst->enabled)
    return;
  spi_hal_init(hw_instance, inst->clk_gpio, inst->miso_gpio, inst->mosi_gpio);
}

static void _peripheral_hal_init_from_config(void)
{
  const hoja_config_s *cfg = hoja_config_get();
  if(!cfg)
    return;

#if defined(HOJA_BSP_HAS_SPI) && (HOJA_BSP_HAS_SPI > 0)
  #if (HOJA_BSP_HAS_SPI >= 1)
    _spi_hal_init_one(0, &cfg->spi.instance_0);
  #endif
  #if (HOJA_BSP_HAS_SPI >= 2)
    _spi_hal_init_one(1, &cfg->spi.instance_1);
  #endif
  #if (HOJA_BSP_HAS_SPI >= 3)
    _spi_hal_init_one(2, &cfg->spi.instance_2);
  #endif
  #if (HOJA_BSP_HAS_SPI >= 4)
    _spi_hal_init_one(3, &cfg->spi.instance_3);
  #endif
#endif

#if defined(HOJA_BSP_HAS_I2C) && (HOJA_BSP_HAS_I2C > 0)
  #if (HOJA_BSP_HAS_I2C >= 1)
    _i2c_hal_init_one(0, &cfg->i2c.instance_0);
  #endif
  #if (HOJA_BSP_HAS_I2C >= 2)
    _i2c_hal_init_one(1, &cfg->i2c.instance_1);
  #endif
  #if (HOJA_BSP_HAS_I2C >= 3)
    _i2c_hal_init_one(2, &cfg->i2c.instance_2);
  #endif
  #if (HOJA_BSP_HAS_I2C >= 4)
    _i2c_hal_init_one(3, &cfg->i2c.instance_3);
  #endif
#endif
}

bool _hoja_null_cb(uint64_t timestamp)
{
  return false;
}

__attribute__((weak)) void cb_hoja_init()
{

}

__attribute__((weak)) void cb_hoja_shutdown()
{

}

__attribute__((weak)) bool cb_hoja_boot_custom_face_mode(const mapper_input_s *in, core_reportformat_t *format_out)
{
  (void)in;
  (void)format_out;
  return false;
}

__attribute__((weak)) uint16_t cb_hoja_read_battery()
{
  return 0xFFFF;
}

__attribute__((weak)) void cb_hoja_read_joystick(uint16_t *input)
{
  (void)&input;
}

__attribute__((weak)) void cb_hoja_read_input(mapper_input_s *input)
{
  (void)&input;
}

volatile bool _deinit_lockout = false;
void hoja_deinit(callback_t cb)
{
  if (_deinit_lockout)
    return;
  _deinit_lockout = true;

  tasks_mark_shutdown();
  sysmon_shutdown();

  // Stop our current loop function
  core_deinit();

  haptics_stop();

  if(!rgb_deinit(cb))
  {
    cb();
  }
}

void hoja_shutdown()
{
  cb_hoja_shutdown();

  battery_set_ship_mode();

  for (;;)
  {
    sys_hal_sleep_ms(5000);
    sys_hal_reboot();
  }
}

void hoja_restart()
{
  core_deinit();
  sys_hal_reboot();
}

// Replace with proper boot function later TODO
bool _core_format_init(void)
{
  // Reload our remap
  mapper_init();
  rgb_init(-1, -1);

  // Reload stick config
  stick_scaling_init();

  if (!core_init())
  {
    rgb_set_pulsing(COLOR_RED);
    return false;
  }

  const boot_info_s *boot = boot_get_info();
  if (boot && (boot->flags & COREBOOT_FLAG_ALTFLASH))
  {
    rgb_set_pulsing(COLOR_ORANGE);
    return true;
  }

  rgb_init(-1, -1);

  return true;
}

static task_s _task_core = {
  .fn = core_task,
  .name = "core",
  .type_mask = (TASK_TYPE_RAPID | TASK_TYPE_RECURRING | TASK_TYPE_SHUTDOWN)
};

static task_s _task_rgb = {
  .fn = rgb_task,
  .name = "rgb",
#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
  .optional_interval_us = RGB_TASK_INTERVAL,
#endif
  .type_mask = (TASK_TYPE_OPTIONAL | TASK_TYPE_RECURRING | TASK_TYPE_SHUTDOWN)
};

static task_s _task_hover = {
  .fn = hover_task,
  .name = "hover",
  .type_mask = (TASK_TYPE_REQUIRED | TASK_TYPE_RECURRING | TASK_TYPE_SHUTDOWN)
};

static task_s _task_macros = {
  .fn = macros_task,
  .name = "macros",
  .optional_interval_us = 8000,
  .type_mask = (TASK_TYPE_OPTIONAL | TASK_TYPE_RECURRING | TASK_TYPE_SHUTDOWN)
};

static task_s _task_flash = {
  .fn = flash_hal_task,
  .name = "flash",
  .optional_interval_us = 16000,
  .type_mask = (TASK_TYPE_OPTIONAL)
};

// Even-spacing interval for extra motion reads within a cycle. Chosen so an
// ~8ms cycle yields 3 evenly-spaced reads (0, ~2.67ms, ~5.33ms) while a 1kHz
// (1ms) cycle only ever takes the guaranteed once-per-cycle read.
#define MOTION_TASK_INTERVAL_US 2667

static task_s _task_motion = {
  .fn = imu_task,
  .name = "imu",
  .motion_interval_us = MOTION_TASK_INTERVAL_US,
  .type_mask = (TASK_TYPE_MOTION)
};

static task_s _task_haptics = {
  .fn = haptics_task,
  .name = "haptics",
  .type_mask = (TASK_TYPE_RAPID | TASK_TYPE_RECURRING)
};

static task_s _task_sysmon = {
  .fn = sysmon_task,
  .name = "sysmon",
  .optional_interval_us = SYSMON_TASK_INTERVAL_US,
  .type_mask = (TASK_TYPE_OPTIONAL | TASK_TYPE_SHUTDOWN)
};

static task_s _task_idle = {
  .fn = idle_manager_task,
  .name = "idle",
  .optional_interval_us = IDLE_TASK_INTERVAL_US,
  .type_mask = (TASK_TYPE_OPTIONAL)
};

static task_s _task_watchdog = {
  .fn = sys_hal_tick,
  .name = "wd",
  .type_mask = (TASK_TYPE_REQUIRED | TASK_TYPE_SHUTDOWN)
};

void _hoja_init_core_tasks(void)
{
  tasks_reset();

  const boot_info_s *boot = boot_get_info();

  tasks_register(&_task_hover);

  switch(boot->reportformat)
  {
    default:
    tasks_register(&_task_haptics);
    break;

    case CORE_REPORTFORMAT_XINPUT:
    case CORE_REPORTFORMAT_SLIPPI:
    case CORE_REPORTFORMAT_GAMECUBE:
    case CORE_REPORTFORMAT_N64:
    tasks_register(&_task_haptics);
    break;

    case CORE_REPORTFORMAT_SINPUT:
    case CORE_REPORTFORMAT_SWPRO:
    tasks_register(&_task_haptics);
    tasks_register(&_task_motion);
    break;

    case CORE_REPORTFORMAT_SNES:
    break;
  }

  // Tasks for all modes
  tasks_register(&_task_flash);
  tasks_register(&_task_idle);
  tasks_register(&_task_macros);
  
  tasks_register(&_task_rgb);
  tasks_register(&_task_sysmon);
  tasks_register(&_task_watchdog);
  tasks_register(&_task_core);
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint64_t c1_timestamp = 0;

  // init core on core 1
  _core_format_init();

  _hoja_init_core_tasks();

  for (;;)
  {
    tasks_run();
  }
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  static uint64_t c0_timestamp = 0;

  // We can lock core 0 for writing to flash
  flash_hal_init();

  for (;;)
  {
    sys_hal_time_us(&c0_timestamp);

    if(!_deinit_lockout)
    {
      // Analog task (any stick half-axis present)
      if(analog_static.axis_lx || analog_static.axis_ly
      || analog_static.axis_rx || analog_static.axis_ry)
        analog_task(c0_timestamp);
    }
  }
}

bool _system_requirements_init()
{
  // System hal init
  sys_hal_init();

  _peripheral_hal_init_from_config();

  // Static config
  static_config_init();

  // System settings
  settings_init();

  // MAIN.C BOARD INIT
  cb_hoja_init();

  return true;
}

bool _system_devices_init(void)
{
  boot_init();

  battery_init();
  fuelgauge_init();
  sysmon_init();
  haptics_init();

  mapper_init();
  rgb_init(-1, -1);
  return true;
}

bool _system_input_init()
{
  // Hover input
  hover_init();

  // Analog joysticks
  analog_init();

  // IMU motion
  imu_init();
  return true;
}

void hoja_init(const hoja_config_s *config)
{
  _hoja_config = config;

  _system_requirements_init();
  _system_input_init();

  _system_devices_init();

  // Init tasks finally
  sys_hal_start_dualcore(_hoja_task_0, _hoja_task_1);
}
