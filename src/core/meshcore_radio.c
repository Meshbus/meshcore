// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_radio.h"

#include "meshcore_platform_bridge.h"

void meshcore_radio_begin(void)
{
  meshcore_platform_bridge_radio_begin();
}

uint32_t meshcore_radio_get_est_airtime(int len_bytes)
{
  return meshcore_platform_bridge_radio_airtime((size_t)len_bytes);
}

float meshcore_radio_packet_score(float snr, int packet_len)
{
  return meshcore_platform_bridge_radio_packet_score((int8_t)(snr * 4.0f),
                                                      (size_t)packet_len);
}

bool meshcore_radio_start_send_raw(const uint8_t *data, int len)
{
  if (data == NULL || len <= 0) {
    return false;
  }

  return meshcore_platform_bridge_radio_packet_send(data, (size_t)len) != 0;
}

bool meshcore_radio_is_in_recv_mode(void)
{
  return meshcore_platform_bridge_radio_in_rx_mode_get();
}

bool meshcore_radio_is_receiving(void)
{
  return meshcore_platform_bridge_radio_receiving_get();
}

void meshcore_radio_trigger_noise_floor_calibrate(int threshold)
{
  meshcore_platform_bridge_radio_noise_floor_calibrate(threshold);
}

void meshcore_radio_reset_agc(void)
{
  meshcore_platform_bridge_radio_agc_reset();
}
