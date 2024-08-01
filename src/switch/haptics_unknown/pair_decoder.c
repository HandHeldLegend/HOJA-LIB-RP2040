#include <stdio.h>
#include <stdint.h>
#include "switch_haptics.h"

// Here's my new decoder that will properly put out
// the uint16_t decoded data pairs :)
void decode_haptic_pairs(uint8_t pattern_type, const uint8_t *encoded_dat, uint16_t *decoded_pairs)
{
uint32_t combined;
  switch(pattern_type) {
  case 0:
  case 1:
  case 2:
  case 3:

    type1
  
    break;
  case 4:
                    /* Type 2 pattern */
    *decoded_pairs =
         ((uint)*(ushort *)((int)encoded_dat + 2) << 18) >> 25 |
         (*(byte *)((int)encoded_dat + 2) & 0x7f) << 8 | 0x8080;
    decoded_pairs[1] =
         (uint)(*(byte *)((int)encoded_dat + 1) >> 1) |
         (((uint)*(ushort *)encoded_dat << 0x17) >> 0x19) << 8 | 0x8080;
    goto switchD_0021f386_caseD_8;
  case 5:
  case 6:
                    /* Type 3 pattern */
    pair5 = ((uint)*(ushort *)((int)encoded_dat + 2) << 0x12) >> 0x19 |
            (uint)(*encoded_dat >> 1) << 8 | 0x8080;
    uVar3 = ((uint)*(byte *)((int)encoded_dat + 2) << 0x19) >> 0x1b;
    bVar5 = -1 < (int)((uint)*encoded_dat << 0x1f);
    uVar4 = pair5;
    if (!bVar5) {
      uVar4 = uVar3;
    }
    *decoded_pairs = uVar4;
    if (bVar5) {
      pair5 = uVar3;
    }
    decoded_pairs[1] = pair5;
    decoded_pairs[2] = ((uint)*(ushort *)((int)encoded_dat + 1) << 0x16) >> 0x1b;
    decoded_pairs[3] = *(byte *)((int)encoded_dat + 1) & 0x1f;
    pair5 = 0x18;
    decoded_pairs[4] = 0x18;
    break;
  case 7:
                    /* Type 4 pattern */
    pair5 = ((uint)*(ushort *)((int)encoded_dat + 2) << 0x12) >> 0x19;
    if ((int)((uint)*encoded_dat << 0x1d) < 0) {
      pair5 = pair5 << 8 | 0x8000;
    }
    else {
      pair5 = pair5 | 0x80;
    }
    bVar5 = -1 < (int)((uint)*encoded_dat << 0x1f);
    uVar4 = pair5;
    if (!bVar5) {
      uVar4 = 0x18;
    }
    *decoded_pairs = uVar4;
    if (bVar5) {
      pair5 = 0x18;
    }
    decoded_pairs[1] = pair5;
    decoded_pairs[2] = ((uint)*(byte *)((int)encoded_dat + 2) << 0x19) >> 0x1b;
    decoded_pairs[3] = ((uint)*(ushort *)((int)encoded_dat + 1) << 0x16) >> 0x1b;
    decoded_pairs[4] = *(byte *)((int)encoded_dat + 1) & 0x1f;
    pair5 = (uint)(*encoded_dat >> 3);
    break;
  default:
    goto switchD_0021f386_caseD_8;
  }
  decoded_pairs[5] = pair5;
}