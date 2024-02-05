/**
 * UnpackAmFmCodes - Unpacks amplitude and frequency modulation codes from motor data
 *
 * @param amFmCodes: Pointer to an array where the unpacked codes will be stored
 * @param motorData: Pointer to the motor data containing packed codes
 *
 * @return: 0 on failure, result code on success
 */
int UnpackAmFmCodes(int *amFmCodes, unsigned char *motorData) {
    // Check if PCM is initialized
    if (!isPcmInitialized) {
        return 0;
    }

    // Calculate vibration count based on available PCM samples
    int v7;
    if (4 * GetAvailablePcmSampleCount() <= 159) {
        v7 = 4 * GetAvailablePcmSampleCount() / 20;
    } else {
        LOBYTE(v7) = 7;
    }
    vibrationCount = v7;

    // Adjust motorData based on device type
    if (deviceType != 1) {
        if (deviceType != 2) {
            return 0;
        }
        motorData += 4;
    }

    // Extract modulation type from motor data
    unsigned char v9 = motorData[3];
    int result = v9 >> 6;
    
    // Handle different modulation types
    int v4;
    if (v9 >> 6) {
        if (result != 1) {
            if (result == 2) {
                if (*(unsigned short *)motorData << 22)
                    v4 = 5;
                else
                    v4 = 2;
            } else if (result == 3) {
                v4 = 3;
            }
            goto LABEL_24;
        }
        if (!((*(unsigned short *)motorData | (motorData[2] << 16)) << 12)) {
            v4 = 1;
            goto LABEL_24;
        }
        int v10 = *motorData;
        if (!(v10 << 30)) {
            v4 = 4;
            goto LABEL_24;
        }
        if ((v10 & 2) != 0) {
            result = 3;
            v4 = 7;
            goto LABEL_24;
        }
        return 0;
    }

    if (!(4 * *(int *)motorData)) {
        return 0;
    }

    result = 3;
    v4 = 6;

LABEL_24:
    vibrationPlayCount = 0;

    // Unpack codes based on modulation type
    switch (v4) {
        case 0:
        case 1:
        case 2:
        case 3:
            amFmCodes[0] = (v9 >> 1) & 0x1F;
            amFmCodes[1] = (*((unsigned short *)motorData + 1) >> 4) & 0x1F;
            amFmCodes[2] = (*(unsigned short *)(motorData + 1) >> 7) & 0x1F;
            amFmCodes[3] = (motorData[1] >> 2) & 0x1F;
            amFmCodes[4] = (*(unsigned short *)motorData >> 5) & 0x1F;
            int v12 = *motorData & 0x1F;
            goto LABEL_35;

        case 4:
            amFmCodes[0] = (*((unsigned short *)motorData + 1) >> 7) & 0x7F | ((motorData[2] & 0x7F) << 8) | 0x8080;
            amFmCodes[1] = (motorData[1] >> 1) | (((*(unsigned short *)motorData >> 2) & 0x7F) << 8) | 0x8080;
            break;

        case 5:
        case 6:
            int v13 = *motorData;
            int v14 = v13 & 1;
            int v15 = (*((unsigned short *)motorData + 1) >> 7) & 0x7F | (v13 >> 1 << 8) | 0x8080;
            int v16 = (motorData[2] >> 2) & 0x1F;
            int v17;
            if (v14)
                v17 = (motorData[2] >> 2) & 0x1F;
            else
                v17 = v15;
            amFmCodes[0] = v17;
            if (!v14)
                v15 = v16;
            amFmCodes[1] = v15;
            amFmCodes[2] = (*(unsigned short *)(motorData + 1) >> 5) & 0x1F;
            amFmCodes[3] = motorData[1] & 0x1F;
            int v12 = 24;
            amFmCodes[4] = 24;
            goto LABEL_35;

        case 7:
            int v18 = *motorData;
            int v19 = v18 & 1;
            int v20 = ((v18 >> 2) & 1) == 0;
            int v21 = (*((unsigned short *)motorData + 1) >> 7) & 0x7F;
            int v22;
            if (v20)
                v22 = v21 | 0x80;
            else
                v22 = (v21 << 8) | 0x8000;
            int v23;
            if (v19)
                v23 = 24;
            else
                v23 = v22;
            amFmCodes[0] = v23;
            if (!v19)
                v22 = 24;
            amFmCodes[1] = v22;
            amFmCodes[2] = (motorData[2] >> 2) & 0x1F;
            amFmCodes[3] = (*(unsigned short *)(motorData + 1) >> 5) & 0x1F;
            amFmCodes[4] = motorData[1] & 0x1F;
            int v12 = *motorData >> 3;

LABEL_35:
            amFmCodes[5] = v12;
            break;

        default:
            break;
    }

    isVibrationPlaying = 0;
    return result;
}

11 1111 1111 1111 1111 1111 1111 1111 1100 0000 0000 0000 0000 0000 11