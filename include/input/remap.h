#ifndef REMAP_H
#define REMAP_H

#include "hoja_includes.h"

void remap_send_data_webusb(input_mode_t mode);
void remap_reset_default(input_mode_t mode);
void remap_set_gc_sp(gc_sp_mode_t sp_mode);
void remap_init(input_mode_t mode, button_data_s *in, button_data_s *out);
void remap_listen_stop();
void remap_listen_enable(input_mode_t mode, mapcode_t mapcode);
void remap_buttons_task();

#endif
