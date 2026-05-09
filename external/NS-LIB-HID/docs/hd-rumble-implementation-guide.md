# HD rumble implementation guide

How **wire bytes**, **indices**, and **optional float reference tables** fit together in **NS-LIB-HID**.

---

## 1. Decode path

- **`ns_lib_haptics_rumble_translate(const uint8_t *data)`**  
  Decodes into **library-internal** state (skips decode when the 30-bit payload matches the previous word), then calls **`ns_set_haptic_indices_cb(samples, count)`** with **`count`** in **0…3**.

Each **`ns_lib_haptic_raw_sample_s`** holds **`hi_amplitude_idx`**, **`lo_amplitude_idx`**, **`hi_frequency_idx`**, **`lo_frequency_idx`**. **`hi` / `lo` amplitude indices** both index the same **`amplitude_linear[256]`** table.

---

## 2. Float reference tables

**`ns_lib_haptics_tables_s`** (see **`ns_lib_haptics.h`**):

- **`amplitude_linear[256]`** — one exp₂ envelope LUT for both channels (rows 0…255).
- **`frequency_hz_hi[128]`**, **`frequency_hz_lo[128]`** — Hz per frequency index.

**`ns_lib_haptics_build_raw_tables()`** fills **`out`** with the same curves **`ns_lib_haptics_init()`** loads internally.

Helpers include **`ns_lib_haptics_freq_index_to_hz_hi` / `_lo`**, **`ns_lib_haptics_exp_lut_index_to_amplitude_linear`**, **`ns_lib_haptics_host_amp_index_to_exp_lut_index`**, **`ns_lib_haptics_get_basic()`**, and optional PCM stepping helpers (**`ns_lib_haptics_hz_to_phase_increment_fp`**, etc.).

---

## 3. Multi-sample timing

Up to **three** samples are sub-steps over roughly **8 ms**. Use them in order (index **0** first).

---

## 4. Files

| Area | Path |
|------|------|
| Types, tables, API | `include/ns_lib_haptics.h` |
| Decode and index callback dispatch | `ns_lib_haptics.c` |
| Weak **`ns_set_haptic_indices_cb`** stub | `ns_lib.c` / **`ns_lib_api.h`** |
