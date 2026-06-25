// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_cayenne_lpp_compat.h"

#include <errno.h>

#define LPP_DIGITAL_INPUT 0U
#define LPP_DIGITAL_OUTPUT 1U
#define LPP_ANALOG_INPUT 2U
#define LPP_ANALOG_OUTPUT 3U
#define LPP_GENERIC_SENSOR 100U
#define LPP_LUMINOSITY 101U
#define LPP_PRESENCE 102U
#define LPP_TEMPERATURE 103U
#define LPP_RELATIVE_HUMIDITY 104U
#define LPP_ACCELEROMETER 113U
#define LPP_BAROMETRIC_PRESSURE 115U
#define LPP_VOLTAGE 116U
#define LPP_CURRENT 117U
#define LPP_FREQUENCY 118U
#define LPP_PERCENTAGE 120U
#define LPP_ALTITUDE 121U
#define LPP_CONCENTRATION 125U
#define LPP_POWER 128U
#define LPP_DISTANCE 130U
#define LPP_ENERGY 131U
#define LPP_DIRECTION 132U
#define LPP_UNIXTIME 133U
#define LPP_GYROMETER 134U
#define LPP_COLOUR 135U
#define LPP_GPS 136U
#define LPP_SWITCH 142U
#define LPP_POLYLINE 240U

#define LPP_DIGITAL_INPUT_SIZE 1U
#define LPP_DIGITAL_OUTPUT_SIZE 1U
#define LPP_ANALOG_INPUT_SIZE 2U
#define LPP_ANALOG_OUTPUT_SIZE 2U
#define LPP_GENERIC_SENSOR_SIZE 4U
#define LPP_LUMINOSITY_SIZE 2U
#define LPP_PRESENCE_SIZE 1U
#define LPP_TEMPERATURE_SIZE 2U
#define LPP_RELATIVE_HUMIDITY_SIZE 1U
#define LPP_ACCELEROMETER_SIZE 6U
#define LPP_BAROMETRIC_PRESSURE_SIZE 2U
#define LPP_VOLTAGE_SIZE 2U
#define LPP_CURRENT_SIZE 2U
#define LPP_FREQUENCY_SIZE 4U
#define LPP_PERCENTAGE_SIZE 1U
#define LPP_ALTITUDE_SIZE 2U
#define LPP_CONCENTRATION_SIZE 2U
#define LPP_POWER_SIZE 2U
#define LPP_DISTANCE_SIZE 4U
#define LPP_ENERGY_SIZE 4U
#define LPP_DIRECTION_SIZE 2U
#define LPP_UNIXTIME_SIZE 4U
#define LPP_GYROMETER_SIZE 6U
#define LPP_COLOUR_SIZE 3U
#define LPP_GPS_SIZE 9U
#define LPP_SWITCH_SIZE 1U

#define LPP_ANALOG_INPUT_MULT 100U
#define LPP_TEMPERATURE_MULT 10U
#define LPP_RELATIVE_HUMIDITY_MULT 2U
#define LPP_BAROMETRIC_PRESSURE_MULT 10U
#define LPP_VOLTAGE_MULT 100U
#define LPP_CURRENT_MULT 1000U
#define LPP_ALTITUDE_MULT 1U
#define LPP_POWER_MULT 1U
#define LPP_DISTANCE_MULT 1000U
#define LPP_GPS_LAT_LON_MULT 10000U
#define LPP_GPS_ALT_MULT 100U
#define LPP_GPS_E4_TO_E6_SCALE 100

static size_t cayenne_lpp_fixed_value_size(uint8_t type)
{
  switch (type) {
  case LPP_DIGITAL_INPUT:
    return LPP_DIGITAL_INPUT_SIZE;
  case LPP_DIGITAL_OUTPUT:
    return LPP_DIGITAL_OUTPUT_SIZE;
  case LPP_ANALOG_INPUT:
    return LPP_ANALOG_INPUT_SIZE;
  case LPP_ANALOG_OUTPUT:
    return LPP_ANALOG_OUTPUT_SIZE;
  case LPP_GENERIC_SENSOR:
    return LPP_GENERIC_SENSOR_SIZE;
  case LPP_LUMINOSITY:
    return LPP_LUMINOSITY_SIZE;
  case LPP_PRESENCE:
    return LPP_PRESENCE_SIZE;
  case LPP_TEMPERATURE:
    return LPP_TEMPERATURE_SIZE;
  case LPP_RELATIVE_HUMIDITY:
    return LPP_RELATIVE_HUMIDITY_SIZE;
  case LPP_ACCELEROMETER:
    return LPP_ACCELEROMETER_SIZE;
  case LPP_BAROMETRIC_PRESSURE:
    return LPP_BAROMETRIC_PRESSURE_SIZE;
  case LPP_VOLTAGE:
    return LPP_VOLTAGE_SIZE;
  case LPP_CURRENT:
    return LPP_CURRENT_SIZE;
  case LPP_FREQUENCY:
    return LPP_FREQUENCY_SIZE;
  case LPP_PERCENTAGE:
    return LPP_PERCENTAGE_SIZE;
  case LPP_ALTITUDE:
    return LPP_ALTITUDE_SIZE;
  case LPP_CONCENTRATION:
    return LPP_CONCENTRATION_SIZE;
  case LPP_POWER:
    return LPP_POWER_SIZE;
  case LPP_DISTANCE:
    return LPP_DISTANCE_SIZE;
  case LPP_ENERGY:
    return LPP_ENERGY_SIZE;
  case LPP_DIRECTION:
    return LPP_DIRECTION_SIZE;
  case LPP_UNIXTIME:
    return LPP_UNIXTIME_SIZE;
  case LPP_GYROMETER:
    return LPP_GYROMETER_SIZE;
  case LPP_COLOUR:
    return LPP_COLOUR_SIZE;
  case LPP_GPS:
    return LPP_GPS_SIZE;
  case LPP_SWITCH:
    return LPP_SWITCH_SIZE;
  default:
    return 0U;
  }
}

static bool cayenne_lpp_value_size(uint8_t type, const uint8_t *value,
                                   size_t remaining, size_t *value_size)
{
  if (value_size == NULL) {
    return false;
  }

  if (type == LPP_POLYLINE) {
    if (value == NULL || remaining == 0U) {
      return false;
    }

    *value_size = value[0];
    return *value_size != 0U && *value_size <= remaining;
  }

  *value_size = cayenne_lpp_fixed_value_size(type);
  return *value_size != 0U && *value_size <= remaining;
}

static int32_t cayenne_lpp_parse_s24_be(const uint8_t *buf)
{
  int32_t value;

  if (buf == NULL) {
    return 0;
  }

  value = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | (int32_t)buf[2];
  if ((value & 0x00800000) != 0) {
    value |= (int32_t)0xFF000000;
  }

  return value;
}

static int cayenne_lpp_add_field(struct cayenne_lpp_writer *writer, uint8_t type,
                                 uint8_t channel, float value, uint8_t size,
                                 uint32_t multiplier, bool is_signed)
{
  uint64_t raw;
  bool negative;

  if (writer == NULL || writer->buf == NULL) {
    return -EINVAL;
  }

  if (size == 0U) {
    return -EINVAL;
  }

  if ((writer->len + (size_t)size + 2U) > writer->cap) {
    return -ENOSPC;
  }

  negative = value < 0.0f;
  if (negative) {
    value = -value;
  }

  raw = (uint64_t)(value * (float)multiplier);
  if (is_signed && negative) {
    uint64_t mask = (1ULL << ((uint64_t)size * 8U)) - 1ULL;

    raw &= mask;
    raw = mask - raw + 1ULL;
    raw &= mask;
  }

  writer->buf[writer->len++] = channel;
  writer->buf[writer->len++] = type;
  for (uint8_t i = 1U; i <= size; i++) {
    writer->buf[writer->len + size - i] = (uint8_t)(raw & 0xFFU);
    raw >>= 8;
  }
  writer->len += size;

  return 0;
}

void cayenne_lpp_init(struct cayenne_lpp_writer *writer, uint8_t *buf,
                      size_t cap)
{
  if (writer == NULL) {
    return;
  }

  writer->buf = buf;
  writer->cap = cap;
  writer->len = 0U;
}

size_t cayenne_lpp_size(const struct cayenne_lpp_writer *writer)
{
  return writer == NULL ? 0U : writer->len;
}

void cayenne_lpp_parse_location(const uint8_t *payload, size_t payload_len,
                                struct cayenne_lpp_location *location)
{
  size_t offset = 0U;

  if (location == NULL) {
    return;
  }

  location->has_latitude = false;
  location->latitude_e6 = 0;
  location->has_longitude = false;
  location->longitude_e6 = 0;

  if (payload == NULL || payload_len == 0U) {
    return;
  }

  while ((offset + 2U) <= payload_len) {
    uint8_t type = payload[offset + 1U];
    size_t value_size;
    const uint8_t *value;

    offset += 2U;
    value = &payload[offset];
    if (!cayenne_lpp_value_size(type, value, payload_len - offset,
                                &value_size)) {
      break;
    }

    if (type == LPP_GPS) {
      int32_t lat_e4 = cayenne_lpp_parse_s24_be(value);
      int32_t lon_e4 = cayenne_lpp_parse_s24_be(value + 3U);

      location->has_latitude = true;
      location->latitude_e6 = lat_e4 * LPP_GPS_E4_TO_E6_SCALE;
      location->has_longitude = true;
      location->longitude_e6 = lon_e4 * LPP_GPS_E4_TO_E6_SCALE;
    }

    offset += value_size;
  }
}

int cayenne_lpp_add_analog_input(struct cayenne_lpp_writer *writer,
                                 uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_ANALOG_INPUT, channel, value,
                               LPP_ANALOG_INPUT_SIZE, LPP_ANALOG_INPUT_MULT,
                               true);
}

int cayenne_lpp_add_barometric_pressure(struct cayenne_lpp_writer *writer,
                                        uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_BAROMETRIC_PRESSURE, channel, value,
                               LPP_BAROMETRIC_PRESSURE_SIZE,
                               LPP_BAROMETRIC_PRESSURE_MULT, false);
}

int cayenne_lpp_add_distance(struct cayenne_lpp_writer *writer,
                             uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_DISTANCE, channel, value,
                               LPP_DISTANCE_SIZE, LPP_DISTANCE_MULT, false);
}

int cayenne_lpp_add_gps(struct cayenne_lpp_writer *writer, uint8_t channel,
                        float latitude, float longitude, float altitude)
{
  int32_t lat;
  int32_t lon;
  int32_t alt;
  uint32_t lat_u;
  uint32_t lon_u;
  uint32_t alt_u;

  if (writer == NULL || writer->buf == NULL) {
    return -EINVAL;
  }

  if ((writer->len + LPP_GPS_SIZE + 2U) > writer->cap) {
    return -ENOSPC;
  }

  lat = (int32_t)(latitude * (float)LPP_GPS_LAT_LON_MULT);
  lon = (int32_t)(longitude * (float)LPP_GPS_LAT_LON_MULT);
  alt = (int32_t)(altitude * (float)LPP_GPS_ALT_MULT);
  lat_u = (uint32_t)lat;
  lon_u = (uint32_t)lon;
  alt_u = (uint32_t)alt;

  writer->buf[writer->len++] = channel;
  writer->buf[writer->len++] = LPP_GPS;
  writer->buf[writer->len++] = (uint8_t)((lat_u >> 16) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)((lat_u >> 8) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)(lat_u & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)((lon_u >> 16) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)((lon_u >> 8) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)(lon_u & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)((alt_u >> 16) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)((alt_u >> 8) & 0xFFU);
  writer->buf[writer->len++] = (uint8_t)(alt_u & 0xFFU);

  return 0;
}

int cayenne_lpp_add_relative_humidity(struct cayenne_lpp_writer *writer,
                                      uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_RELATIVE_HUMIDITY, channel, value,
                               LPP_RELATIVE_HUMIDITY_SIZE,
                               LPP_RELATIVE_HUMIDITY_MULT, false);
}

int cayenne_lpp_add_temperature(struct cayenne_lpp_writer *writer,
                                uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_TEMPERATURE, channel, value,
                               LPP_TEMPERATURE_SIZE, LPP_TEMPERATURE_MULT,
                               true);
}

int cayenne_lpp_add_voltage(struct cayenne_lpp_writer *writer, uint8_t channel,
                            float value)
{
  return cayenne_lpp_add_field(writer, LPP_VOLTAGE, channel, value,
                               LPP_VOLTAGE_SIZE, LPP_VOLTAGE_MULT, false);
}

int cayenne_lpp_add_current(struct cayenne_lpp_writer *writer, uint8_t channel,
                            float value)
{
  return cayenne_lpp_add_field(writer, LPP_CURRENT, channel, value,
                               LPP_CURRENT_SIZE, LPP_CURRENT_MULT, false);
}

int cayenne_lpp_add_power(struct cayenne_lpp_writer *writer, uint8_t channel,
                          float value)
{
  return cayenne_lpp_add_field(writer, LPP_POWER, channel, value,
                               LPP_POWER_SIZE, LPP_POWER_MULT, false);
}

int cayenne_lpp_add_altitude(struct cayenne_lpp_writer *writer,
                             uint8_t channel, float value)
{
  return cayenne_lpp_add_field(writer, LPP_ALTITUDE, channel, value,
                               LPP_ALTITUDE_SIZE, LPP_ALTITUDE_MULT, true);
}
