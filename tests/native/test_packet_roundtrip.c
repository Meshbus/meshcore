// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <string.h>

#include "meshcore_packet.h"

int main(void)
{
  struct meshcore_packet source;
  struct meshcore_packet parsed;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t len;
  const uint8_t payload[] = {0x10U, 0x20U, 0x30U, 0x40U};

  meshcore_packet_init(&source);
  source.header = (uint8_t)((MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM
                             << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  source.path_len = 0U;
  memcpy(source.payload, payload, sizeof(payload));
  source.payload_len = sizeof(payload);

  len = meshcore_packet_write_to(&source, raw);
  NATIVE_TEST_ASSERT_EQ(6U, len);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&parsed, raw, len));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM,
                        meshcore_packet_get_payload_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(sizeof(payload), parsed.payload_len);
  NATIVE_TEST_ASSERT(memcmp(parsed.payload, payload, sizeof(payload)) == 0);

  return 0;
}
