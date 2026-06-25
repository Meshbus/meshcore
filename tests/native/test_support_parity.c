// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <errno.h>
#include <string.h>

#include "meshcore_packet_manager.h"
#include "meshcore_cayenne_lpp_compat.h"
#include "meshcore_txt_data.h"

/*
 * Layer 1 support parity evidence:
 * - .reference/meshcore/src/helpers/StaticPoolPacketManager.h
 * - .reference/meshcore/src/helpers/StaticPoolPacketManager.cpp
 * - .reference/meshcore/src/helpers/BaseChatMesh.cpp telemetry payload usage
 * - .reference/meshcore/src/helpers/TxtDataHelpers.h
 * - .reference/meshcore/src/helpers/TxtDataHelpers.cpp
 */

static int test_packet_queue_manager_fixed_pool_and_full_pool_behavior(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_packet *first;
  struct meshcore_packet *second;
  struct meshcore_packet *third;

  meshcore_packet_queue_manager_prepare(&manager, 2);
  NATIVE_TEST_ASSERT(manager.initialized);
  NATIVE_TEST_ASSERT_EQ(2, meshcore_packet_queue_manager_get_free_count(&manager));

  first = meshcore_packet_queue_manager_alloc_new(&manager);
  second = meshcore_packet_queue_manager_alloc_new(&manager);
  third = meshcore_packet_queue_manager_alloc_new(&manager);
  NATIVE_TEST_ASSERT(first != NULL);
  NATIVE_TEST_ASSERT(second != NULL);
  NATIVE_TEST_ASSERT(third == NULL);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_free_count(&manager));

  meshcore_packet_queue_manager_free(&manager, first);
  NATIVE_TEST_ASSERT_EQ(1, meshcore_packet_queue_manager_get_free_count(&manager));
  third = meshcore_packet_queue_manager_alloc_new(&manager);
  NATIVE_TEST_ASSERT(third == first);

  meshcore_packet_queue_manager_free(&manager, second);
  meshcore_packet_queue_manager_free(&manager, third);
  meshcore_packet_queue_manager_deinit(&manager);
  return 0;
}

static int test_packet_queue_priority_and_schedule_order(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_packet *late_high;
  struct meshcore_packet *ready_low;
  struct meshcore_packet *ready_high;
  struct meshcore_packet *picked;
  uint32_t scheduled_for = 0U;

  meshcore_packet_queue_manager_prepare(&manager, 3);
  late_high = meshcore_packet_queue_manager_alloc_new(&manager);
  ready_low = meshcore_packet_queue_manager_alloc_new(&manager);
  ready_high = meshcore_packet_queue_manager_alloc_new(&manager);
  NATIVE_TEST_ASSERT(late_high != NULL);
  NATIVE_TEST_ASSERT(ready_low != NULL);
  NATIVE_TEST_ASSERT(ready_high != NULL);

  meshcore_packet_queue_manager_queue_outbound(&manager, late_high, 0U, 50U);
  meshcore_packet_queue_manager_queue_outbound(&manager, ready_low, 5U, 10U);
  meshcore_packet_queue_manager_queue_outbound(&manager, ready_high, 1U, 10U);

  NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_outbound_count(&manager, 9U));
  NATIVE_TEST_ASSERT_EQ(2, meshcore_packet_queue_manager_get_outbound_count(&manager, 10U));
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_next_outbound_scheduled_get(
      &manager, 9U, &scheduled_for));
  NATIVE_TEST_ASSERT_EQ(10U, scheduled_for);
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_next_outbound_scheduled_get(
      &manager, 10U, &scheduled_for));
  NATIVE_TEST_ASSERT_EQ(10U, scheduled_for);

  picked = meshcore_packet_queue_manager_get_next_outbound(&manager, 10U);
  NATIVE_TEST_ASSERT(picked == ready_high);
  picked = meshcore_packet_queue_manager_get_next_outbound(&manager, 10U);
  NATIVE_TEST_ASSERT(picked == ready_low);
  picked = meshcore_packet_queue_manager_get_next_outbound(&manager, 49U);
  NATIVE_TEST_ASSERT(picked == NULL);
  picked = meshcore_packet_queue_manager_get_next_outbound(&manager, 50U);
  NATIVE_TEST_ASSERT(picked == late_high);

  meshcore_packet_queue_manager_free(&manager, ready_high);
  meshcore_packet_queue_manager_free(&manager, ready_low);
  meshcore_packet_queue_manager_free(&manager, late_high);
  meshcore_packet_queue_manager_deinit(&manager);
  return 0;
}

static int test_packet_queue_inbound_schedule_order(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_packet *first;
  struct meshcore_packet *second;
  uint32_t scheduled_for = 0U;

  meshcore_packet_queue_manager_prepare(&manager, 2);
  first = meshcore_packet_queue_manager_alloc_new(&manager);
  second = meshcore_packet_queue_manager_alloc_new(&manager);
  NATIVE_TEST_ASSERT(first != NULL);
  NATIVE_TEST_ASSERT(second != NULL);

  meshcore_packet_queue_manager_queue_inbound(&manager, first, 25U);
  meshcore_packet_queue_manager_queue_inbound(&manager, second, 10U);
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_next_inbound_scheduled_get(
      &manager, 1U, &scheduled_for));
  NATIVE_TEST_ASSERT_EQ(10U, scheduled_for);
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_get_next_inbound(&manager,
                                                                    9U) == NULL);
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_get_next_inbound(&manager,
                                                                    10U) == second);
  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_get_next_inbound(&manager,
                                                                    25U) == first);

  meshcore_packet_queue_manager_free(&manager, first);
  meshcore_packet_queue_manager_free(&manager, second);
  meshcore_packet_queue_manager_deinit(&manager);
  return 0;
}

static int test_cayenne_lpp_writer_golden_vector_and_capacity_errors(void)
{
  struct cayenne_lpp_writer writer;
  uint8_t buf[64];
  const uint8_t expected[] = {
    0x01U, 0x02U, 0xFFU, 0x85U,
    0x02U, 0x67U, 0x00U, 0xFEU,
    0x03U, 0x68U, 0x64U,
    0x04U, 0x88U, 0x01U, 0xE2U, 0x40U, 0xFDU, 0xFDU, 0xC7U, 0x00U, 0xC3U, 0x50U,
  };

  cayenne_lpp_init(&writer, buf, sizeof(buf));
  NATIVE_TEST_ASSERT_EQ(0, cayenne_lpp_add_analog_input(&writer, 1U, -1.23f));
  NATIVE_TEST_ASSERT_EQ(0, cayenne_lpp_add_temperature(&writer, 2U, 25.4f));
  NATIVE_TEST_ASSERT_EQ(0, cayenne_lpp_add_relative_humidity(&writer, 3U, 50.0f));
  NATIVE_TEST_ASSERT_EQ(0, cayenne_lpp_add_gps(&writer, 4U, 12.3456f,
                                               -13.1641f, 500.0f));
  NATIVE_TEST_ASSERT_EQ(sizeof(expected), cayenne_lpp_size(&writer));
  NATIVE_TEST_ASSERT(memcmp(buf, expected, sizeof(expected)) == 0);

  cayenne_lpp_init(&writer, buf, 3U);
  NATIVE_TEST_ASSERT_EQ((uint32_t)-ENOSPC,
                        (uint32_t)cayenne_lpp_add_temperature(&writer, 1U, 1.0f));
  NATIVE_TEST_ASSERT_EQ(0U, cayenne_lpp_size(&writer));
  NATIVE_TEST_ASSERT_EQ((uint32_t)-EINVAL,
                        (uint32_t)cayenne_lpp_add_voltage(NULL, 1U, 1.0f));

  return 0;
}

static int test_cayenne_lpp_location_parser_boundaries(void)
{
  struct cayenne_lpp_location location;
  const uint8_t payload[] = {
    0x01U, 0x67U, 0x00U, 0x01U,
    0x04U, 0x88U, 0x01U, 0xE2U, 0x40U, 0xFDU, 0xFDU, 0xC7U, 0x00U, 0xC3U, 0x50U,
  };
  const uint8_t truncated_gps[] = {
    0x04U, 0x88U, 0x00U, 0x1EU, 0x24U,
  };

  cayenne_lpp_parse_location(payload, sizeof(payload), &location);
  NATIVE_TEST_ASSERT(location.has_latitude);
  NATIVE_TEST_ASSERT(location.has_longitude);
  NATIVE_TEST_ASSERT_EQ(12345600, location.latitude_e6);
  NATIVE_TEST_ASSERT_EQ((uint32_t)-13164100, (uint32_t)location.longitude_e6);

  cayenne_lpp_parse_location(truncated_gps, sizeof(truncated_gps), &location);
  NATIVE_TEST_ASSERT(!location.has_latitude);
  NATIVE_TEST_ASSERT(!location.has_longitude);

  location.has_latitude = true;
  location.has_longitude = true;
  cayenne_lpp_parse_location(NULL, 1U, &location);
  NATIVE_TEST_ASSERT(!location.has_latitude);
  NATIVE_TEST_ASSERT(!location.has_longitude);

  return 0;
}

static int test_txt_helpers_match_upstream_string_and_hex_semantics(void)
{
  char dest[6];
  char padded[6];
  char ftoa[16];

  memset(dest, 'x', sizeof(dest));
  meshcore_txt_strncpy(dest, "abcdef", sizeof(dest));
  NATIVE_TEST_ASSERT(memcmp(dest, "abcde", sizeof(dest)) == 0);

  memset(padded, 'x', sizeof(padded));
  meshcore_txt_strzcpy(padded, "ab", sizeof(padded));
  NATIVE_TEST_ASSERT_EQ('a', padded[0]);
  NATIVE_TEST_ASSERT_EQ('b', padded[1]);
  NATIVE_TEST_ASSERT_EQ('\0', padded[2]);
  NATIVE_TEST_ASSERT_EQ('\0', padded[3]);
  NATIVE_TEST_ASSERT_EQ('\0', padded[4]);
  NATIVE_TEST_ASSERT_EQ('\0', padded[5]);

  NATIVE_TEST_ASSERT(meshcore_txt_is_blank(""));
  NATIVE_TEST_ASSERT(meshcore_txt_is_blank("   "));
  NATIVE_TEST_ASSERT(!meshcore_txt_is_blank("  x"));

  NATIVE_TEST_ASSERT_EQ(0x1A2BU, meshcore_txt_from_hex("1a2B"));
  NATIVE_TEST_ASSERT_EQ(0x12U, meshcore_txt_from_hex("12zz34"));
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_txt_from_hex("not-hex"));

  NATIVE_TEST_ASSERT_EQ(0U, MESHCORE_TXT_TYPE_PLAIN);
  NATIVE_TEST_ASSERT_EQ(1U, MESHCORE_TXT_TYPE_CLI_DATA);
  NATIVE_TEST_ASSERT_EQ(2U, MESHCORE_TXT_TYPE_SIGNED_PLAIN);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_DATA_TYPE_RESERVED,
                        MESHCORE_TXT_DATA_TYPE_RESERVED);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_DATA_TYPE_DEV,
                        MESHCORE_TXT_DATA_TYPE_DEV);

  NATIVE_TEST_ASSERT(strcmp(meshcore_txt_ftoa3(1.230f, ftoa, sizeof(ftoa)),
                            "1.23") == 0);
  NATIVE_TEST_ASSERT(strcmp(meshcore_txt_ftoa3(-1.234f, ftoa, sizeof(ftoa)),
                            "-1.234") == 0);

  return 0;
}

int main(void)
{
  NATIVE_TEST_ASSERT_EQ(0, test_packet_queue_manager_fixed_pool_and_full_pool_behavior());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_queue_priority_and_schedule_order());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_queue_inbound_schedule_order());
  NATIVE_TEST_ASSERT_EQ(0, test_cayenne_lpp_writer_golden_vector_and_capacity_errors());
  NATIVE_TEST_ASSERT_EQ(0, test_cayenne_lpp_location_parser_boundaries());
  NATIVE_TEST_ASSERT_EQ(0, test_txt_helpers_match_upstream_string_and_hex_semantics());

  return 0;
}
