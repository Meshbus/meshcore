// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_TELEMETRY_CAYENNE_LPP_COMPAT_H_
#define MESHCORE_SUPPORT_TELEMETRY_CAYENNE_LPP_COMPAT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal Cayenne LPP compatibility helpers used by telemetry payload logic.
 */

struct cayenne_lpp_writer {
  uint8_t *buf;
  size_t cap;
  size_t len;
};

struct cayenne_lpp_location {
  bool has_latitude;
  int32_t latitude_e6;
  bool has_longitude;
  int32_t longitude_e6;
};

void cayenne_lpp_init(struct cayenne_lpp_writer *writer, uint8_t *buf,
                      size_t cap);
size_t cayenne_lpp_size(const struct cayenne_lpp_writer *writer);
void cayenne_lpp_parse_location(const uint8_t *payload, size_t payload_len,
                                struct cayenne_lpp_location *location);

int cayenne_lpp_add_analog_input(struct cayenne_lpp_writer *writer,
                                 uint8_t channel, float value);
int cayenne_lpp_add_barometric_pressure(struct cayenne_lpp_writer *writer,
                                        uint8_t channel, float value);
int cayenne_lpp_add_distance(struct cayenne_lpp_writer *writer,
                             uint8_t channel, float value);
int cayenne_lpp_add_gps(struct cayenne_lpp_writer *writer, uint8_t channel,
                        float latitude, float longitude, float altitude);
int cayenne_lpp_add_relative_humidity(struct cayenne_lpp_writer *writer,
                                      uint8_t channel, float value);
int cayenne_lpp_add_temperature(struct cayenne_lpp_writer *writer,
                                uint8_t channel, float value);
int cayenne_lpp_add_voltage(struct cayenne_lpp_writer *writer, uint8_t channel,
                            float value);
int cayenne_lpp_add_current(struct cayenne_lpp_writer *writer, uint8_t channel,
                            float value);
int cayenne_lpp_add_power(struct cayenne_lpp_writer *writer, uint8_t channel,
                          float value);
int cayenne_lpp_add_altitude(struct cayenne_lpp_writer *writer,
                             uint8_t channel, float value);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_TELEMETRY_CAYENNE_LPP_COMPAT_H_ */
