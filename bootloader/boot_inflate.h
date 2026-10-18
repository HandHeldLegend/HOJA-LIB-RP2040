#ifndef BOOTLOADER_BOOT_INFLATE_H
#define BOOTLOADER_BOOT_INFLATE_H

// Raw deflate (RFC 1951) decoding for staged updates: the bare stream, without zlib's or gzip's
// wrapper, as a browser's CompressionStream("deflate-raw") makes it.

#include <stdbool.h>
#include <stdint.h>

// Inflates len bytes from in, handing each output byte to put in order; put returns false to stop
// the stream (it is full). False for a stream that is malformed, cut short or stopped. Not
// reentrant: the window and code tables are static.
bool boot_inflate(const uint8_t *in, uint32_t len, bool (*put)(uint8_t value));

#endif
