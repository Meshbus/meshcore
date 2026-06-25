// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_advert_data.h"

#include <string.h>

static int32_t meshcore_advert_data_scale_coord(double value)
{
  return (int32_t)(value * 1000000.0);
}

void meshcore_advert_data_builder_init(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type)
{
  builder->type = adv_type;
  builder->has_loc = false;
  builder->name = NULL;
  builder->lat = 0;
  builder->lon = 0;
  builder->feat1 = 0;
  builder->feat2 = 0;
}

void meshcore_advert_data_builder_init_with_name(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type,
    const char *name)
{
  meshcore_advert_data_builder_init(builder, adv_type);
  builder->name = name;
}

void meshcore_advert_data_builder_init_with_name_lat_lon(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type,
    const char *name, double lat, double lon)
{
  meshcore_advert_data_builder_init_with_name(builder, adv_type, name);
  builder->has_loc = true;
  builder->lat = meshcore_advert_data_scale_coord(lat);
  builder->lon = meshcore_advert_data_scale_coord(lon);
}

void meshcore_advert_data_builder_set_feat1(
    struct meshcore_advert_data_builder *builder, uint16_t feat1)
{
  builder->feat1 = feat1;
}

void meshcore_advert_data_builder_set_feat2(
    struct meshcore_advert_data_builder *builder, uint16_t feat2)
{
  builder->feat2 = feat2;
}

uint8_t meshcore_advert_data_builder_encode_to(
    const struct meshcore_advert_data_builder *builder, uint8_t app_data[])
{
  int i = 1;

  app_data[0] = builder->type;

  if (builder->has_loc) {
    app_data[0] |= ADV_LATLON_MASK;
    memcpy(&app_data[i], &builder->lat, sizeof(builder->lat));
    i += (int)sizeof(builder->lat);
    memcpy(&app_data[i], &builder->lon, sizeof(builder->lon));
    i += (int)sizeof(builder->lon);
  }
  if (builder->feat1 != 0U) {
    app_data[0] |= ADV_FEAT1_MASK;
    memcpy(&app_data[i], &builder->feat1, sizeof(builder->feat1));
    i += (int)sizeof(builder->feat1);
  }
  if (builder->feat2 != 0U) {
    app_data[0] |= ADV_FEAT2_MASK;
    memcpy(&app_data[i], &builder->feat2, sizeof(builder->feat2));
    i += (int)sizeof(builder->feat2);
  }
  if (builder->name != NULL && builder->name[0] != '\0') {
    const char *sp = builder->name;

    app_data[0] |= ADV_NAME_MASK;
    while (*sp != '\0' && i < (int)MESHCORE_MAX_ADVERT_DATA_LEN) {
      app_data[i++] = (uint8_t)*sp++;
    }
  }

  return (uint8_t)i;
}

void meshcore_advert_data_parser_init(
    struct meshcore_advert_data_parser *parser, const uint8_t app_data[],
    uint8_t app_data_len)
{
  size_t i = 1U;

  if (parser == NULL) {
    return;
  }

  parser->name[0] = '\0';
  parser->lat = 0;
  parser->lon = 0;
  parser->flags = 0U;
  parser->valid = false;
  parser->feat1 = 0;
  parser->feat2 = 0;

  if (app_data == NULL || app_data_len == 0U) {
    return;
  }

  parser->flags = app_data[0];
  if ((parser->flags & ADV_LATLON_MASK) != 0U) {
    if ((size_t)app_data_len < i + sizeof(parser->lat) + sizeof(parser->lon)) {
      return;
    }
    memcpy(&parser->lat, &app_data[i], sizeof(parser->lat));
    i += sizeof(parser->lat);
    memcpy(&parser->lon, &app_data[i], sizeof(parser->lon));
    i += sizeof(parser->lon);
  }
  if ((parser->flags & ADV_FEAT1_MASK) != 0U) {
    if ((size_t)app_data_len < i + sizeof(parser->feat1)) {
      return;
    }
    memcpy(&parser->feat1, &app_data[i], sizeof(parser->feat1));
    i += sizeof(parser->feat1);
  }
  if ((parser->flags & ADV_FEAT2_MASK) != 0U) {
    if ((size_t)app_data_len < i + sizeof(parser->feat2)) {
      return;
    }
    memcpy(&parser->feat2, &app_data[i], sizeof(parser->feat2));
    i += sizeof(parser->feat2);
  }

  if ((size_t)app_data_len < i) {
    return;
  }
  if ((parser->flags & ADV_NAME_MASK) != 0U) {
    size_t nlen = (size_t)app_data_len - i;

    if (nlen >= sizeof(parser->name)) {
      return;
    }
    if (nlen > 0U) {
      memcpy(parser->name, &app_data[i], nlen);
    }
    parser->name[nlen] = '\0';
  }

  parser->valid = true;
}

bool meshcore_advert_data_parser_is_valid(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->valid;
}

uint8_t meshcore_advert_data_parser_get_type(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->flags & 0x0FU;
}

uint16_t meshcore_advert_data_parser_get_feat1(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->feat1;
}

uint16_t meshcore_advert_data_parser_get_feat2(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->feat2;
}

bool meshcore_advert_data_parser_has_name(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->name[0] != '\0';
}

const char *meshcore_advert_data_parser_get_name(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->name;
}

bool meshcore_advert_data_parser_has_lat_lon(
    const struct meshcore_advert_data_parser *parser)
{
  return (parser->flags & ADV_LATLON_MASK) != 0U;
}

int32_t meshcore_advert_data_parser_get_int_lat(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->lat;
}

int32_t meshcore_advert_data_parser_get_int_lon(
    const struct meshcore_advert_data_parser *parser)
{
  return parser->lon;
}

double meshcore_advert_data_parser_get_lat(
    const struct meshcore_advert_data_parser *parser)
{
  return ((double)parser->lat) / 1000000.0;
}

double meshcore_advert_data_parser_get_lon(
    const struct meshcore_advert_data_parser *parser)
{
  return ((double)parser->lon) / 1000000.0;
}
