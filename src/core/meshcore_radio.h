// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_RADIO_H_
#define MESHCORE_CORE_RADIO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal radio boundary mapped from upstream Dispatcher.h Radio.
 */

void meshcore_radio_begin(void);
uint32_t meshcore_radio_get_est_airtime(int len_bytes);
float meshcore_radio_packet_score(float snr, int packet_len);
bool meshcore_radio_start_send_raw(const uint8_t *data, int len);
bool meshcore_radio_is_in_recv_mode(void);
bool meshcore_radio_is_receiving(void);
void meshcore_radio_trigger_noise_floor_calibrate(int threshold);
void meshcore_radio_reset_agc(void);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_RADIO_H_ */
