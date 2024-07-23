#include <stdio.h>
#include <stdint.h>



/* There is a vibration playback function which is called on a timer callback
PlayVibration() is called, 
sensor measurements are retrieved (IMU/Accel)
Then the remaining PCM samples are calculated which is how many available samples are left?
If the remaining sample count is less than 5, GetNextVibration() is called
*/

typedef struct
{
    uint8_t channel; // vibeCtrl[0]
    uint8_t enabled; // vibeCtrl[1]
    uint8_t padding[2];
    uint8_t sampleShift; // vibeCtrl[4]
    uint8_t padding2[2];
    void *pcmDataPtr; // *(undefined4 *)(vibeCtrl + 8)
} VibrationControl_s;

typedef struct
{
    uint16_t words[8];
} PCMLookupTable_s;

VibrationControl_s vibeCtrlGlobal = {0};
VibrationControl_s* PTR_DAT_0021f534 = &vibeCtrlGlobal;

void PlayVibration(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  ushort pcmValue;
  uint space1;
  uint space2;
  undefined4 intStatus;
  int rawSample;
  uint processIdx;
  uint adjustedSample;
  PCMLookupTable_s *pcmArray;
  VibrationControl_s *vibeCtrl;
  
  vibeCtrl = PTR_DAT_0021f534;
  if (vibeCtrl->enabled != 0) {
    space1 = UkyoPcm::getUsedSpace(uchar)
                       (*(undefined4 *)(PTR_DAT_0021f534 + 8),*PTR_DAT_0021f534,param_3,param_4,
                        param_4);
    space2 = UkyoPcm::getUsedSpace(uchar)(*(undefined4 *)(vibeCtrl + 8),*vibeCtrl ^ 1);
    if (space1 < space2) {
      space2 = space1;
    }
                    /* Check that we have 4 or greater space available */
    if (3 < space2) {
      intStatus = _tx_v7m_get_int();
      _tx_v7m_disable_int();
                    /* Enable Vibration Bit */
      _DAT_00320000 = _DAT_00320000 | 0x1000;
      space1 = 0;
      while ( (space1 <= space2 - 4 && (rawSample = GetAvailablePcmSampleCount(), rawSample != 0)) ) {
        rawSample = GetPcmSample();
        if (rawSample == 0x7fff) {
          adjustedSample = 128;
        }
        else {
          adjustedSample = rawSample + 0x8000 >> (0x10 - (byte)vibeCtrl[4] & 0xff) & 0xffff;
        }
                    /* vibeCtrl[0] or first struct member */
        *vibeCtrl = 1;
        pcmArray = PCMSamplesArray;
        processIdx = 0;
        do {
          if (adjustedSample < 65) {
            pcmValue = *(ushort *)(pcmArray + processIdx * 2 + adjustedSample * 16);
          }
          else {
            pcmValue = ~*(ushort *)(pcmArray + processIdx * 2 + (128 - adjustedSample) * 16);
          }
          UkyoPcm::dataWrite(ushort*,ushort,uchar)(*(undefined4 *)(vibeCtrl + 8),pcmValue,*vibeCtrl)
          ;
          //*vibeCtrl = *vibeCtrl ^ 1;
          vibeCtrl->channel = vibeCtrl->channel ^ 1; // Toggle channel
          processIdx = processIdx + 1 & 0xffff;
        } while (processIdx < 8);
        space1 = space1 + 4 & 0xffff;
      }
      _DAT_00320000 = _DAT_00320000 & 0xffffefff;
      _tx_v7m_set_int(intStatus);
      return;
    }
  }
  return;
}

#define PCM_BUFFER_SIZE 160
int pcm_samples[PCM_BUFFER_SIZE];
int read_idx = 0;
int write_idx = 0;

#define PTR_DAT_00221360 (pcm_samples) // Value points to our PCM samples buffer
#define PTR_DAT_0021f534 (DAT_002234b8)

int GetAvailablePcmSampleCount(void)
{
  int available_samples = read_idx - write_idx;
  if(available_samples < 0)
  {
    available_samples += PCM_BUFFER_SIZE;
  }
  
  return available_samples;
}

void GetNextVibration(void)

{
  undefined *puVar1;
  
  puVar1 = PTR_DAT_0021f534;
  if (PTR_DAT_0021f534[1] != 0) {
    PTR_DAT_0021f534[5] = 1;
    if (*(ushort *)(puVar1 + 6) < 0xc9) {
      GetNextPcmSampleAddress();
      processAudioSamples();
      *(short *)(puVar1 + 6) = *(short *)(puVar1 + 6) + 1;
    }
    else {
      GetNextPcmSampleAddress();
      processScaleAudioSamples();
    }
    UpdatePcmSampleCount();
    return;
  }
  return;
}

byte UnpackAmFmCodes(int16_t *decoded_array, uint8_t *encoded_data)

{
  uint8_t bVar1;
  undefined *puVar2;
  undefined uVar3;
  uint8_t haptic_category;
  int *puVar4;
  int pcm_available;
  int *puVar5;
  uint uVar6;
  uint uVar7;
  undefined4 pattern_type;
  uint uVar8;
  bool bVar9;
  
  puVar2 = PTR_DAT_0021f534;
  puVar5 = encoded_data;
  puVar4 = decoded_array;
  pattern_type = 0;
  if (PTR_DAT_0021f534[1] == 0) {
    return 0;
  }
  pcm_available = GetAvailablePcmSampleCount();
  if (pcm_available * -4 + 0x9f < 0 == SBORROW4(0x9f,pcm_available * 4)) {
    pcm_available = GetAvailablePcmSampleCount();
    uVar3 = (undefined)((pcm_available << 2) / 20);
  }
  else {
    uVar3 = 7;
  }
  puVar2[3] = uVar3;
  if (*controller_type != 1) {
    if (*controller_type != 2) {
      return 0;
    }
    puVar5 = puVar5 + 1;
  }
  bVar1 = *(byte *)((int)puVar5 + 3);
  haptic_category = bVar1 >> 6;
  if (haptic_category == 0) {
    if ((*puVar5 & 0x3fffffff) == 0) {
      return 0;
    }
    haptic_category = 3;
    pattern_type = 6;
  }
  else if (haptic_category == 1) {
    if ((*(uint3 *)puVar5 & 0xfffff) == 0) {
      pattern_type = 1;
    }
    else if ((uint)*(byte *)puVar5 * 0x40000000 == 0) {
      pattern_type = 4;
    }
    else {
      if (-1 < (int)((uint)*(byte *)puVar5 * 0x40000000)) {
        return 0;
      }
      haptic_category = 3;
      pattern_type = 7;
    }
  }
  else if (haptic_category == 2) {
    if ((*(ushort *)puVar5 & 0x3ff) == 0) {
      pattern_type = 2;
    }
    else {
      pattern_type = 5;
    }
  }
  else if (haptic_category == 3) {
    pattern_type = 3;
  }
  *(undefined2 *)(puVar2 + 6) = 0;
  switch(pattern_type) {
  case 0:
  case 1:
  case 2:
  case 3:
    *puVar4 = ((uint)bVar1 << 0x1a) >> 0x1b;
    puVar4[1] = ((uint)*(ushort *)((int)puVar5 + 2) << 0x17) >> 0x1b;
    puVar4[2] = ((uint)*(ushort *)((int)puVar5 + 1) << 0x14) >> 0x1b;
    puVar4[3] = ((uint)*(byte *)((int)puVar5 + 1) << 0x19) >> 0x1b;
    puVar4[4] = ((uint)*(ushort *)puVar5 << 0x16) >> 0x1b;
    uVar6 = *(byte *)puVar5 & 0x1f;
    break;
  case 4:
    *puVar4 = ((uint)*(ushort *)((int)puVar5 + 2) << 0x12) >> 0x19 |
              (*(byte *)((int)puVar5 + 2) & 0x7f) << 8 | 0x8080;
    puVar4[1] = (uint)(*(byte *)((int)puVar5 + 1) >> 1) |
                (((uint)*(ushort *)puVar5 << 0x17) >> 0x19) << 8 | 0x8080;
    goto switchD_0021f386_caseD_8;
  case 5:
  case 6:
    uVar6 = ((uint)*(ushort *)((int)puVar5 + 2) << 0x12) >> 0x19 | (uint)(*(byte *)puVar5 >> 1) << 8
            | 0x8080;
    uVar7 = ((uint)*(byte *)((int)puVar5 + 2) << 0x19) >> 0x1b;
    bVar9 = -1 < (int)((uint)*(byte *)puVar5 << 0x1f);
    uVar8 = uVar6;
    if (!bVar9) {
      uVar8 = uVar7;
    }
    *puVar4 = uVar8;
    if (bVar9) {
      uVar6 = uVar7;
    }
    puVar4[1] = uVar6;
    puVar4[2] = ((uint)*(ushort *)((int)puVar5 + 1) << 0x16) >> 0x1b;
    puVar4[3] = *(byte *)((int)puVar5 + 1) & 0x1f;
    uVar6 = 0x18;
    puVar4[4] = 0x18;
    break;
  case 7:
    uVar6 = ((uint)*(ushort *)((int)puVar5 + 2) << 0x12) >> 0x19;
    if ((int)((uint)*(byte *)puVar5 << 0x1d) < 0) {
      uVar6 = uVar6 << 8 | 0x8000;
    }
    else {
      uVar6 = uVar6 | 0x80;
    }
    bVar9 = -1 < (int)((uint)*(byte *)puVar5 << 0x1f);
    uVar8 = uVar6;
    if (!bVar9) {
      uVar8 = 0x18;
    }
    *puVar4 = uVar8;
    if (bVar9) {
      uVar6 = 0x18;
    }
    puVar4[1] = uVar6;
    puVar4[2] = ((uint)*(byte *)((int)puVar5 + 2) << 0x19) >> 0x1b;
    puVar4[3] = ((uint)*(ushort *)((int)puVar5 + 1) << 0x16) >> 0x1b;
    puVar4[4] = *(byte *)((int)puVar5 + 1) & 0x1f;
    uVar6 = (uint)(*(byte *)puVar5 >> 3);
    break;
  default:
    goto switchD_0021f386_caseD_8;
  }
  puVar4[5] = uVar6;
switchD_0021f386_caseD_8:
  puVar2[5] = 0;
  return haptic_category;
}

/*
   Returns a pointer to the next PCM sample address in a circular buffer.
   The buffer contains 160 samples (320 bytes), with each sample being 2 bytes.
*/
uint16_t *GetNextPcmSampleAddress(void)
{
    // Base address of the circular buffer
    uint16_t *base_address = PTR_DAT_0022135c;

    // Read the current index from a specific offset in a base address
    int current_index = *(int *)(DAT_00221360 + 0xc);

    // Compute the circular buffer index (0 to 159)
    int circular_index = current_index % 160;

    // Calculate the address offset (circular_index * 2 bytes per sample)
    int address_offset = circular_index * 2;

    // Compute the final address by adding the offset to the base address
    return base_address + address_offset;
}

void vibrationProcess(uint8_t *encoded_haptic_bytes)

{
    int haptic_category;
    int free_pcm_samples;
    int stored_interrupt;
    int pcm_ptr_address;
    int packet_check;
    unsigned int decoded_data[7];

    packet_check = 0;
    haptic_category = UnpackAmFmCodes(decoded_data, encoded_haptic_bytes);

    if (haptic_category != 0)
    {
        /* While there are 20 or more samples free, more samples are processed */
        while (free_pcm_samples = GetFreePcmSampleCount(), 19 < free_pcm_samples)
        {

            /* Interrupt is stored for later and disabled */
            stored_interrupt = _tx_v7m_get_int();
            _tx_v7m_disable_int();

            /* Grab the address of the next free sample pointer */
            pcm_ptr_address = GetNextPcmSampleAddress();

            int *encoded_data_address = decoded_data + (packet_check * 2);

            process_samples_1(pcm_ptr_address, encoded_data_address);

            UpdatePcmSampleCount();

            /* Enable ISR again */
            _tx_v7m_set_int(stored_interrupt);
            packet_check = packet_check + 1;
            if (packet_check == haptic_category)
            {
                return;
            }
            if (packet_check == 0)
            {
                return;
            }
        }
    }
    return;
}

#define DAT_00221580 (0x00225D58)

int *DAT_0022157c = NULL;

void process_samples_1(int pcm_ptr_address, unsigned int *decoded_pair)
{
    int sample_unknown;
    int base_address;
    int base_address_2;
    short scaled_sample;

    // Initialize base addresses and decode audio data
    base_address = DAT_00221580;
    *DAT_0022157c = 1;
    decodeAudioData(base_address - 0xd4, base_address - 0x50, 0x14, decoded_pair);

    base_address_2 = DAT_00221580 - 80;

    // Process samples and fill the output
    for (int i = 0; i < 20; i++)
    {
        // Calculate address and read the sample value
        int current_address = base_address_2 + i * 4;
        sample_unknown = *(int *)current_address;

        // Determine scaled_sample based on the value of sample_unknown
        if (sample_unknown < 0)
        {
            if (-sample_unknown < 0x40000000)
            {
                scaled_sample = (short)(-sample_unknown >> 15);
            }
            else
            {
                scaled_sample = 32767;
            }
            scaled_sample = -scaled_sample;
        }
        else
        {
            if (sample_unknown < 0x40000000)
            {
                scaled_sample = (short)(sample_unknown >> 15);
            }
            else
            {
                scaled_sample = 32767;
            }
        }

        // Store the scaled sample at the output address
        *(short *)(pcm_ptr_address + i * 2) = scaled_sample;
    }
}
