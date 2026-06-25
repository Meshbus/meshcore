// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <string.h>

#include "meshcore_advert_data.h"
#include "meshcore_packet.h"

/*
 * Layer 1 parity evidence:
 * - .reference/meshcore/src/Packet.h
 * - .reference/meshcore/src/Packet.cpp
 * - .reference/meshcore/src/helpers/AdvertDataHelpers.h
 * - .reference/meshcore/src/helpers/AdvertDataHelpers.cpp
 */

static int test_public_payload_constants_match_packet_core(void)
{
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_REQ,
                        MESHCORE_PACKET_PAYLOAD_TYPE_REQ);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RESPONSE,
                        MESHCORE_PACKET_PAYLOAD_TYPE_RESPONSE);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        MESHCORE_PACKET_PAYLOAD_TYPE_TXT_MSG);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ACK,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ACK);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ADVERT,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ADVERT);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_TXT,
                        MESHCORE_PACKET_PAYLOAD_TYPE_GRP_TXT);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_DATA,
                        MESHCORE_PACKET_PAYLOAD_TYPE_GRP_DATA);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ANON_REQ,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ANON_REQ);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_PATH,
                        MESHCORE_PACKET_PAYLOAD_TYPE_PATH);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TRACE,
                        MESHCORE_PACKET_PAYLOAD_TYPE_TRACE);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_MULTIPART,
                        MESHCORE_PACKET_PAYLOAD_TYPE_MULTIPART);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_CONTROL,
                        MESHCORE_PACKET_PAYLOAD_TYPE_CONTROL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RAW_CUSTOM,
                        MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM);
  NATIVE_TEST_ASSERT_EQ(PATH_EXTRA_TYPE_SNR,
                        MESHCORE_PACKET_PATH_EXTRA_TYPE_SNR);
  NATIVE_TEST_ASSERT_EQ(0xFFU, MESHCORE_OUT_PATH_UNKNOWN);

  return 0;
}

static int test_packet_path_length_boundaries_match_upstream_encoding(void)
{
  struct meshcore_packet packet;
  uint8_t raw[] = {
    (uint8_t)((PAYLOAD_TYPE_RAW_CUSTOM << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT),
    0xC0U,
    0x01U,
  };

  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len(0U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len(63U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len((1U << 6) | 32U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len((2U << 6) | 21U));

  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((1U << 6) | 33U));
  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((2U << 6) | 22U));
  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((3U << 6) | 0U));

  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_RAW_CUSTOM << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  packet.path_len = (1U << 6) | 33U;
  packet.payload[0] = 0xAAU;
  packet.payload_len = 1U;
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_packet_write_to(&packet, raw));

  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(!meshcore_packet_read_from(&packet, raw, sizeof(raw)));

  return 0;
}

static int test_packet_transport_code_serialization_is_little_endian(void)
{
  struct meshcore_packet source;
  struct meshcore_packet parsed;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t len;
  const uint8_t expected[] = {
    (uint8_t)((PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT) |
              ROUTE_TYPE_TRANSPORT_DIRECT),
    0x34U,
    0x12U,
    0xCDU,
    0xABU,
    0x00U,
    0x99U,
  };

  meshcore_packet_init(&source);
  source.header = expected[0];
  source.transport_codes[0] = 0x1234U;
  source.transport_codes[1] = 0xABCDU;
  source.payload[0] = 0x99U;
  source.payload_len = 1U;

  len = meshcore_packet_write_to(&source, raw);
  NATIVE_TEST_ASSERT_EQ(sizeof(expected), len);
  NATIVE_TEST_ASSERT(memcmp(raw, expected, sizeof(expected)) == 0);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&parsed, raw, len));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_TRANSPORT_DIRECT,
                        meshcore_packet_get_route_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(0x1234U, parsed.transport_codes[0]);
  NATIVE_TEST_ASSERT_EQ(0xABCDU, parsed.transport_codes[1]);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&parsed));

  return 0;
}

static int test_advert_data_golden_vector_and_parser_boundaries(void)
{
  struct meshcore_advert_data_builder builder;
  struct meshcore_advert_data_parser parser;
  uint8_t encoded[MESHCORE_MAX_ADVERT_DATA_LEN];
  const uint8_t expected[] = {
    0xF1U,
    0x87U, 0xD6U, 0x12U, 0x00U,
    0x60U, 0xDAU, 0xD9U, 0xFFU,
    0x34U, 0x12U,
    0xCDU, 0xABU,
    'n', 'o', 'd', 'e',
  };
  const uint8_t truncated_lat_lon[] = {0x11U, 0x00U, 0x00U, 0x00U};
  uint8_t too_long_name[MESHCORE_MAX_ADVERT_DATA_LEN + 1U];
  uint8_t len;

  meshcore_advert_data_builder_init_with_name_lat_lon(
      &builder, ADV_TYPE_CHAT, "node", 1.234567, -2.5);
  meshcore_advert_data_builder_set_feat1(&builder, 0x1234U);
  meshcore_advert_data_builder_set_feat2(&builder, 0xABCDU);
  len = meshcore_advert_data_builder_encode_to(&builder, encoded);

  NATIVE_TEST_ASSERT_EQ(sizeof(expected), len);
  NATIVE_TEST_ASSERT(memcmp(encoded, expected, sizeof(expected)) == 0);

  meshcore_advert_data_parser_init(&parser, encoded, len);
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_is_valid(&parser));
  NATIVE_TEST_ASSERT_EQ(ADV_TYPE_CHAT,
                        meshcore_advert_data_parser_get_type(&parser));
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_has_lat_lon(&parser));
  NATIVE_TEST_ASSERT_EQ(1234567, meshcore_advert_data_parser_get_int_lat(&parser));
  NATIVE_TEST_ASSERT_EQ((uint32_t)-2500000,
                        (uint32_t)meshcore_advert_data_parser_get_int_lon(&parser));
  NATIVE_TEST_ASSERT_EQ(0x1234U,
                        meshcore_advert_data_parser_get_feat1(&parser));
  NATIVE_TEST_ASSERT_EQ(0xABCDU,
                        meshcore_advert_data_parser_get_feat2(&parser));
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_has_name(&parser));
  NATIVE_TEST_ASSERT(strcmp(meshcore_advert_data_parser_get_name(&parser),
                            "node") == 0);

  meshcore_advert_data_parser_init(&parser, truncated_lat_lon,
                                   sizeof(truncated_lat_lon));
  NATIVE_TEST_ASSERT(!meshcore_advert_data_parser_is_valid(&parser));

  memset(too_long_name, 'a', sizeof(too_long_name));
  too_long_name[0] = ADV_TYPE_CHAT | ADV_NAME_MASK;
  meshcore_advert_data_parser_init(&parser, too_long_name,
                                   (uint8_t)sizeof(too_long_name));
  NATIVE_TEST_ASSERT(!meshcore_advert_data_parser_is_valid(&parser));

  return 0;
}

int main(void)
{
  NATIVE_TEST_ASSERT_EQ(0, test_public_payload_constants_match_packet_core());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_path_length_boundaries_match_upstream_encoding());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_transport_code_serialization_is_little_endian());
  NATIVE_TEST_ASSERT_EQ(0, test_advert_data_golden_vector_and_parser_boundaries());

  return 0;
}
