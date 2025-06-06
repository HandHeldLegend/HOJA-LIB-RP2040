#ifndef UTILITIES_CALLBACK_H
#define UTILITIES_CALLBACK_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*callback_t)(void);
typedef void (*time_callback_t)(uint64_t);
typedef bool(*hid_report_tunnel_cb)(uint8_t report_id, const void *report, uint16_t len);
typedef void (*hid_task_tunnel_cb)(uint64_t timestamp, hid_report_tunnel_cb cb);

#endif