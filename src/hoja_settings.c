#include "hoja_settings.h"

#define FLASH_TARGET_OFFSET (1200 * 1024)
#define SETTINGS_BANK_B_OFFSET (FLASH_TARGET_OFFSET)
#define SETTINGS_BANK_A_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE)
#define SETTINGS_BANK_C_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + FLASH_SECTOR_SIZE)
#define BANK_A_NUM 0
#define BANK_B_NUM 1
#define BANK_C_NUM 2 // Bank for power or battery configuration

hoja_settings_vcurrent_s global_loaded_settings = {0};
hoja_settings_battery_storage_s global_loaded_battery_storage = {.charge_level=1000};
bool global_loaded_settings_bank = BANK_A_NUM;

void _settings_validate()
{
  // Validate some settings
  global_loaded_settings.gc_sp_light_trigger = (global_loaded_settings.gc_sp_light_trigger < 50) ? 50 : global_loaded_settings.gc_sp_light_trigger;
  global_loaded_settings.switch_mac_address[0] &= 0xFE;
  global_loaded_settings.switch_mac_address[5] = 0x9B;
}

void _settings_migrate(const uint8_t *old, int old_version, hoja_settings_vcurrent_s *new)
{
  new->settings_version = HOJA_SETTINGS_VERSION;

  // Migrate v1 settings to new format
  if(old_version == 1) {
    hoja_settings_v1_s *m = (hoja_settings_v1_s *)old;
    new->input_mode = m->input_mode;
    memcpy(new->switch_mac_address, m->switch_mac_address, sizeof(m->switch_mac_address));
    new->lx_center = m->lx_center;
    new->ly_center = m->ly_center;
    new->rx_center = m->rx_center;
    new->ry_center = m->ry_center;
    memcpy(new->l_angles, m->l_angles, sizeof(m->l_angles));
    memcpy(new->r_angles, m->r_angles, sizeof(m->r_angles));
    memcpy(new->l_angle_distances, m->l_angle_distances, sizeof(m->l_angle_distances));
    memcpy(new->r_angle_distances, m->r_angle_distances, sizeof(m->r_angle_distances));
    memcpy(new->imu_0_offsets, m->imu_0_offsets, sizeof(m->imu_0_offsets));
    memcpy(new->imu_1_offsets, m->imu_1_offsets, sizeof(m->imu_1_offsets));
    memcpy(new->rgb_colors, m->rgb_colors, sizeof(m->rgb_colors));
    new->remap_switch = m->remap_switch;
    new->remap_xinput = m->remap_xinput;
    new->remap_gamecube = m->remap_gamecube;
    new->remap_n64 = m->remap_n64;
    new->remap_snes = m->remap_snes;
    new->gc_sp_mode = m->gc_sp_mode;
    new->rumble_intensity = m->rumble_intensity;
    new->gc_sp_light_trigger = m->gc_sp_light_trigger;
    new->rumble_mode = m->rumble_mode;
    memcpy(new->switch_host_address, m->switch_host_address, sizeof(m->switch_host_address));
    memcpy(new->l_sub_angles, m->l_sub_angles, sizeof(m->l_sub_angles));
    memcpy(new->r_sub_angles, m->r_sub_angles, sizeof(m->r_sub_angles));
    new->rgb_mode = m->rgb_mode;
    memcpy(new->rainbow_colors, m->rainbow_colors, sizeof(m->rainbow_colors));
    new->rgb_step_speed = m->rgb_step_speed;
    new->deadzone_left_center = m->deadzone_left_center;
    new->deadzone_left_outer = m->deadzone_left_outer;
    new->deadzone_right_center = m->deadzone_right_center;
    new->deadzone_right_outer = m->deadzone_right_outer;
    new->trigger_l.lower = m->trigger_l_lower;
    new->trigger_l.upper = m->trigger_l_upper;
    new->trigger_r.lower = m->trigger_r_lower;
    new->trigger_r.upper = m->trigger_r_upper;
  }
}

int _settings_get_config_version(uint16_t version)
{
    switch(version)
    {
        default:
            return -1;
            break;
        case 0xA001:
            return 1;
            break;
        case 0xA002:
            return 2;
            break;
    }
}

// Internal functions for command processing
void _generate_mac()
{
  printf("Generated MAC: ");
  for (uint8_t i = 0; i < 6; i++)
  {
    global_loaded_settings.switch_mac_address[i] = get_rand_32() & 0xFF;
    if (!i)
      global_loaded_settings.switch_mac_address[i] &= 0xFE;
    printf("%X : ", global_loaded_settings.switch_mac_address[i]);
  }

  global_loaded_settings.switch_mac_address[5] = 0x9B;
  printf("\n");
}

void _settings_init_default()
{
  const hoja_settings_vcurrent_s set = {
      .settings_version = HOJA_SETTINGS_VERSION,
      .input_mode = INPUT_MODE_SWPRO,
      .lx_center = 2048,
      .ly_center = 2048,
      .rx_center = 2048,
      .ry_center = 2048,
      .l_angles = {0, 45, 90, 135, 180, 225, 270, 315},
      .r_angles = {0, 45, 90, 135, 180, 225, 270, 315},
      .l_angle_distances = {
          600,
          600,
          600,
          600,
          600,
          600,
          600,
          600,
      },
      .r_angle_distances = {
          600,
          600,
          600,
          600,
          600,
          600,
          600,
          600,
      },
      .rgb_colors = HOJA_RGB_DEFAULTS,
      .rumble_intensity = 50,
      .rumble_mode = RUMBLE_TYPE_ERM,
      .gc_sp_light_trigger    = 50,
      .deadzone_left_outer    = 50,
      .deadzone_left_center   = 100,
      .deadzone_right_outer   = 50,
      .deadzone_right_center  = 100,
  };
  memcpy(&global_loaded_settings, &set, sizeof(hoja_settings_vcurrent_s));

  remap_reset_default(INPUT_MODE_SWPRO);
  remap_reset_default(INPUT_MODE_XINPUT);
  remap_reset_default(INPUT_MODE_GAMECUBE);
  remap_reset_default(INPUT_MODE_N64);
  remap_reset_default(INPUT_MODE_SNES);
  memset(global_loaded_settings.imu_0_offsets, 0, sizeof(int8_t) * 6);
  memset(global_loaded_settings.imu_1_offsets, 0, sizeof(int8_t) * 6);
  _generate_mac();

  global_loaded_settings.rainbow_colors[0] = COLOR_RED.color;
  global_loaded_settings.rainbow_colors[1] = COLOR_ORANGE.color;
  global_loaded_settings.rainbow_colors[2] = COLOR_YELLOW.color;
  global_loaded_settings.rainbow_colors[3] = COLOR_GREEN.color;
  global_loaded_settings.rainbow_colors[4] = COLOR_BLUE.color;
  global_loaded_settings.rainbow_colors[5] = COLOR_PURPLE.color;
}


bool settings_get_bank()
{
  return global_loaded_settings_bank;
}

// Returns true if loaded ok
// returns false if no settings and reset to default
bool settings_load()
{
  static_assert(sizeof(hoja_settings_vcurrent_s) <= FLASH_SECTOR_SIZE);
  static_assert(sizeof(hoja_settings_v1_s) <= FLASH_SECTOR_SIZE);
  const uint8_t *read_bank_a = (const uint8_t *)(XIP_BASE + SETTINGS_BANK_A_OFFSET);
  const uint8_t *read_bank_b = (const uint8_t *)(XIP_BASE + SETTINGS_BANK_B_OFFSET);

  // Determine which version each bank contains
  hoja_settings_version_parse_s vpa  = {0};
  hoja_settings_version_parse_s vpb  = {0};
  memcpy(&vpa, read_bank_a, sizeof(hoja_settings_version_parse_s));
  memcpy(&vpb, read_bank_b, sizeof(hoja_settings_version_parse_s));
  int va = _settings_get_config_version(vpa.settings_version);
  int vb = _settings_get_config_version(vpb.settings_version);

  // Battery bank data
  static_assert(sizeof(hoja_settings_battery_storage_s) <= FLASH_SECTOR_SIZE);
  const uint8_t *read_bank_c = (const uint8_t *)(XIP_BASE + SETTINGS_BANK_C_OFFSET); // Battery storage
  memcpy(&global_loaded_battery_storage, read_bank_c, sizeof(hoja_settings_battery_storage_s));
  // Set defaults if not set
  if(global_loaded_battery_storage.magic != HOJA_DEVICE_ID)
  {
    global_loaded_battery_storage.magic         = HOJA_DEVICE_ID;
    global_loaded_battery_storage.charge_level  = 1000;
    global_loaded_battery_storage.max_depletion_time = 0;
  }
  // End battery bank default data setting

  // Set up some flags we can use
  bool up_to_date = false;
  bool no_config = false;
  bool must_migrate = false;
  bool migrate_bank = BANK_A_NUM;

  // Check if we have no configuration data
  if((va==-1) && (vb==-1))
  {
    no_config = true;
  }
  // Check if we have an up to date config data
  else if((va==2) || (vb==2))
  {
    up_to_date = true;
    if(va==2)
    {
      memcpy(&global_loaded_settings, read_bank_a, sizeof(hoja_settings_vcurrent_s));
      global_loaded_settings_bank = BANK_A_NUM;
    }
    else if(vb==2)
    {
      memcpy(&global_loaded_settings, read_bank_b, sizeof(hoja_settings_vcurrent_s));
      global_loaded_settings_bank = BANK_B_NUM;
    }
  }
  // Check if any config data is out of date
  else if((va<2) || (vb<2))
  {
    must_migrate = true;
    // Set the bank we are migrating FROM
    migrate_bank = (va<2) ? BANK_A_NUM : BANK_B_NUM;
  }

  // Handle flags
  if(no_config)
  {
    global_loaded_settings_bank = BANK_A_NUM;
    _settings_init_default();
    settings_save_from_core0(false);
    return false;
  }
  else if(must_migrate)
  {
    if(migrate_bank == BANK_A_NUM)
    {
      global_loaded_settings_bank = BANK_B_NUM;
      _settings_migrate(read_bank_a, va, &global_loaded_settings);
    }
    else
    {
      global_loaded_settings_bank = BANK_A_NUM;
      _settings_migrate(read_bank_b, vb, &global_loaded_settings);
    }
    settings_save_from_core0(false);
    return false;
  }

  _settings_validate();

  return true;
}

volatile bool _save_flag = false;
volatile bool _save_battery_flag = false;
volatile bool _webusb_indicate = false;

void settings_set_charge_level(uint16_t charge_level)
{
  global_loaded_battery_storage.charge_level = charge_level;
}

void settings_set_max_depletion_time(uint16_t max_depletion_time)
{
  global_loaded_battery_storage.max_depletion_time = max_depletion_time;
}

void settings_save_battery_from_core0()
{
  _save_battery_flag = true;

  // Block until it's done
  while(!_save_battery_flag)
  {
    sleep_us(1);
  }
}

void settings_save_battery_from_core1()
{
  _save_battery_flag = true;
  settings_core1_save_check();
}

void settings_core1_save_check()
{
  if (_save_flag)
  {
    multicore_lockout_start_blocking();

    // Store interrupts status and disable
    uint32_t ints = save_and_disable_interrupts();

    // Calculate storage bank address via index
    uint32_t memoryAddress = (global_loaded_settings_bank==BANK_A_NUM) ? SETTINGS_BANK_A_OFFSET : SETTINGS_BANK_B_OFFSET;

    // Create blank page data
    uint8_t page[FLASH_SECTOR_SIZE] = {0x00};
    // Copy settings into our page buffer
    memcpy(page, &global_loaded_settings, sizeof(hoja_settings_vcurrent_s));

    // Erase the settings flash sector
    flash_range_erase(memoryAddress, FLASH_SECTOR_SIZE);

    // Program the flash sector with our page
    flash_range_program(memoryAddress, page, FLASH_SECTOR_SIZE);

    // Restore interrups
    restore_interrupts(ints);
    multicore_lockout_end_blocking();

    // Indicate change
    if (_webusb_indicate)
    {
      webusb_save_confirm();
      _webusb_indicate = false;
    }
    _save_flag = false;
  }

  if(_save_battery_flag)
  {
    multicore_lockout_start_blocking();

    // Store interrupts status and disable
    uint32_t ints = save_and_disable_interrupts();

    // Calculate storage bank address via index
    uint32_t memoryAddress = SETTINGS_BANK_C_OFFSET;

    // Create blank page data
    uint8_t page[FLASH_SECTOR_SIZE] = {0x00};
    // Copy settings into our page buffer
    memcpy(page, &global_loaded_battery_storage, sizeof(hoja_settings_battery_storage_s));

    // Erase the settings flash sector
    flash_range_erase(memoryAddress, FLASH_SECTOR_SIZE);

    // Program the flash sector with our page
    flash_range_program(memoryAddress, page, FLASH_SECTOR_SIZE);

    // Restore interrups
    restore_interrupts(ints);
    multicore_lockout_end_blocking();

    _save_battery_flag = false;
  }
}

void settings_save_webindicate()
{
  _webusb_indicate = true;
}

void settings_save_from_core0()
{
  _save_flag = true;
}

void settings_set_rumble(uint8_t intensity, rumble_type_t type)
{
  intensity = (intensity > 100) ? 100 : intensity;
  global_loaded_settings.rumble_intensity = intensity;

  cb_hoja_rumble_init();
}

void settings_set_deadzone(uint8_t selection, uint16_t value)
{
  switch (selection)
  {
  // Left inner
  case 0:
    global_loaded_settings.deadzone_left_center = value;
    break;

  // Left outer
  case 1:
    global_loaded_settings.deadzone_left_outer = value;
    break;

  // Right inner
  case 2:
    global_loaded_settings.deadzone_right_center = value;
    break;

  // Right outer
  case 3:
    global_loaded_settings.deadzone_right_outer = value;
    break;
  }

  // Init sticks again
  stick_scaling_init();
}

void settings_set_centers(int lx, int ly, int rx, int ry)
{
  global_loaded_settings.lx_center.center = lx;
  global_loaded_settings.ly_center.center = ly;
  global_loaded_settings.rx_center.center = rx;
  global_loaded_settings.ry_center.center = ry;
}

void settings_set_analog_inversion(bool lx, bool ly, bool rx, bool ry)
{
  global_loaded_settings.lx_center.invert = lx;
  global_loaded_settings.ly_center.invert = ly;
  global_loaded_settings.rx_center.invert = rx;
  global_loaded_settings.ry_center.invert = ry;
}

void settings_set_distances(float *l_angle_distances, float *r_angle_distances)
{
  memcpy(global_loaded_settings.l_angle_distances, l_angle_distances, sizeof(float) * 8);
  memcpy(global_loaded_settings.r_angle_distances, r_angle_distances, sizeof(float) * 8);
}

void settings_set_angles(float *l_angles, float *r_angles)
{
  memcpy(global_loaded_settings.l_angles, l_angles, sizeof(float) * 8);
  memcpy(global_loaded_settings.r_angles, r_angles, sizeof(float) * 8);
}

void settings_set_mode(input_mode_t mode)
{
  mode = (mode >= INPUT_MODE_MAX) ? 0 : mode;
  
  global_loaded_settings.input_mode = mode;
}