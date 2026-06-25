// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_ADVERT_DATA_H_
#define MESHCORE_SUPPORT_ADVERT_DATA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal advert app-data encoding and decoding helpers.
 */

#define ADV_TYPE_NONE 0U
#define ADV_TYPE_CHAT 1U
#define ADV_TYPE_REPEATER 2U
#define ADV_TYPE_ROOM 3U
#define ADV_TYPE_SENSOR 4U

#define ADV_LATLON_MASK 0x10U
#define ADV_FEAT1_MASK 0x20U
#define ADV_FEAT2_MASK 0x40U
#define ADV_NAME_MASK 0x80U

#define MESHCORE_MAX_ADVERT_DATA_LEN 32U

struct meshcore_advert_data_builder {
  uint8_t type;
  bool has_loc;
  const char *name;
  int32_t lat;
  int32_t lon;
  uint16_t feat1;
  uint16_t feat2;
};

struct meshcore_advert_data_parser {
  uint8_t flags;
  bool valid;
  char name[MESHCORE_MAX_ADVERT_DATA_LEN];
  int32_t lat;
  int32_t lon;
  uint16_t feat1;
  uint16_t feat2;
};

void meshcore_advert_data_builder_init(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type);
void meshcore_advert_data_builder_init_with_name(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type,
    const char *name);
void meshcore_advert_data_builder_init_with_name_lat_lon(
    struct meshcore_advert_data_builder *builder, uint8_t adv_type,
    const char *name, double lat, double lon);
void meshcore_advert_data_builder_set_feat1(
    struct meshcore_advert_data_builder *builder, uint16_t feat1);
void meshcore_advert_data_builder_set_feat2(
    struct meshcore_advert_data_builder *builder, uint16_t feat2);
uint8_t meshcore_advert_data_builder_encode_to(
    const struct meshcore_advert_data_builder *builder, uint8_t app_data[]);

void meshcore_advert_data_parser_init(
    struct meshcore_advert_data_parser *parser, const uint8_t app_data[],
    uint8_t app_data_len);
bool meshcore_advert_data_parser_is_valid(
    const struct meshcore_advert_data_parser *parser);
uint8_t meshcore_advert_data_parser_get_type(
    const struct meshcore_advert_data_parser *parser);
uint16_t meshcore_advert_data_parser_get_feat1(
    const struct meshcore_advert_data_parser *parser);
uint16_t meshcore_advert_data_parser_get_feat2(
    const struct meshcore_advert_data_parser *parser);
bool meshcore_advert_data_parser_has_name(
    const struct meshcore_advert_data_parser *parser);
const char *meshcore_advert_data_parser_get_name(
    const struct meshcore_advert_data_parser *parser);
bool meshcore_advert_data_parser_has_lat_lon(
    const struct meshcore_advert_data_parser *parser);
int32_t meshcore_advert_data_parser_get_int_lat(
    const struct meshcore_advert_data_parser *parser);
int32_t meshcore_advert_data_parser_get_int_lon(
    const struct meshcore_advert_data_parser *parser);
double meshcore_advert_data_parser_get_lat(
    const struct meshcore_advert_data_parser *parser);
double meshcore_advert_data_parser_get_lon(
    const struct meshcore_advert_data_parser *parser);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_ADVERT_DATA_H_ */
