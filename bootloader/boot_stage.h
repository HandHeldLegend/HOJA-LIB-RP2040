#ifndef BOOTLOADER_BOOT_STAGE_H
#define BOOTLOADER_BOOT_STAGE_H

// A staged update (see boot_layout.h): checked, written into place (copied, or inflated from raw
// deflate) and checked again, then its header erased. A power cut part way leaves the header, so
// the next boot starts again from the beginning: the staged data is never written over.

void boot_stage_apply(void);

#endif
