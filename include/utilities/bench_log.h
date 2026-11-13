#ifndef UTILITIES_BENCH_LOG_H
#define UTILITIES_BENCH_LOG_H

// Bench only (HOJA_BENCH_LOG): text kept in RAM and read out over the config interface, for boards
// with no serial port to print to. Without the flag, writes go nowhere.
//
// Reading it changes timing: once anything talks to the config interface the firmware streams input
// to it, and the main loop waits on every packet the PC has not collected. Read the log after a
// timing measurement, not during one (a reader that paused 50 ms between reads cut a 64 reports/s
// Bluetooth link to 18).

#include <stddef.h>
#include <stdint.h>

// Any context. Never waits: when full, the oldest text makes room.
void bench_log_write(const char *text, size_t len);

// Up to max bytes of the oldest unread text: how many were copied
uint16_t bench_log_read(uint8_t *out, uint16_t max);

#endif
