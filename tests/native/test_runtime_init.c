// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"
#include "fake_platform.h"

#include <string.h>

#include "meshcore/runtime.h"
#include "meshcore_group_channel.h"
#include "meshcore_packet.h"
#include "meshcore_runtime_bridge.h"

static int test_channel_text_target_uses_secret_prefix(void)
{
  struct meshcore_group_channel channel = {0};
  struct meshcore_packet packet;
  uint8_t data[] = {
      0x78U, 0x56U, 0x34U, 0x12U,
      0x00U,
      's', 'e', 'n', 'd', 'e', 'r', ':', ' ', 'h', 'i',
  };
  const meshcore_common_message_t *message;
  size_t i;

  meshcore_native_platform_reset();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());

  channel.hash[0] = 0x5AU;
  for (i = 0U; i < sizeof(channel.secret); i++) {
    channel.secret[i] = (uint8_t)(0xB0U + i);
  }
  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_FLOOD);
  packet.snr_q4 = 8;

  meshcore_runtime_on_group_data_recv(&packet, PAYLOAD_TYPE_GRP_TXT, &channel,
                                      data, sizeof(data));

  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_message_count_get());
  message = meshcore_native_platform_last_message_get();
  NATIVE_TEST_ASSERT_EQ(MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_CHANNEL,
                        message->type);
  NATIVE_TEST_ASSERT(message->has_rx_snr);
  NATIVE_TEST_ASSERT(message->rx_snr == 2.0f);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MESSAGE_TARGET_PREFIX_BYTES,
                        message->target_len);
  NATIVE_TEST_ASSERT(memcmp(message->target, channel.secret,
                            MESHCORE_MESSAGE_TARGET_PREFIX_BYTES) == 0);
  NATIVE_TEST_ASSERT(message->target[0] != channel.hash[0]);
  NATIVE_TEST_ASSERT_EQ(2U, message->payload_len);
  NATIVE_TEST_ASSERT(memcmp(message->payload, "hi", 2U) == 0);

  meshcore_deinit();
  return 0;
}

static int test_binary_response_rejects_oversized_flood_return(void)
{
  meshcore_common_binary_request_event_t request = {0};
  uint8_t payload[MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN];

  memset(payload, 0x55, sizeof(payload));
  request.route = MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD;
  request.tag = 0x12345678U;
  request.payload_len = 1U;
  request.path_len = 0U;

  NATIVE_TEST_ASSERT(meshcore_node_binary_response(&request, payload,
                                                   sizeof(payload)) < 0);

  request.route = MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_response(&request, payload,
                                                         sizeof(payload)));

  return 0;
}

int main(void)
{
  meshcore_native_platform_reset();
  meshcore_native_platform_identity_get_fail_set(true);
  NATIVE_TEST_ASSERT(meshcore_init() < 0);

  meshcore_native_platform_reset();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  NATIVE_TEST_ASSERT(meshcore_native_platform_timer_arm_count_get() > 0U);
  NATIVE_TEST_ASSERT(meshcore_native_platform_last_timer_deadline_get() > 0U);

  NATIVE_TEST_ASSERT_EQ(0, test_binary_response_rejects_oversized_flood_return());

  meshcore_deinit();
  return test_channel_text_target_uses_secret_prefix();
}
