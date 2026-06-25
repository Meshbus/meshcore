// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"
#include "fake_platform.h"

#include <string.h>

#include "meshcore/runtime.h"
#include "meshcore_packet.h"

/*
 * Layer 2 runtime oracle evidence:
 * - .reference/meshcore/src/helpers/BaseChatMesh.h
 * - .reference/meshcore/src/helpers/BaseChatMesh.cpp
 * - .reference/meshcore/src/Dispatcher.h
 * - .reference/meshcore/src/Dispatcher.cpp
 * - .reference/meshcore/src/MeshCore.h
 * - .reference/meshcore/src/helpers/ContactInfo.h
 * - .reference/meshcore/src/helpers/ChannelDetails.h
 * - .reference/meshcore/examples/companion_radio/NodePrefs.h
 * - .reference/meshcore/examples/companion_radio/DataStore.h
 * - .reference/meshcore/examples/companion_radio/DataStore.cpp
 * - .reference/meshcore/examples/companion_radio/MyMesh.h
 * - .reference/meshcore/examples/companion_radio/MyMesh.cpp
 */

/* MESHCORE_API_COVERAGE: meshcore_init meshcore_deinit meshcore_timer_fired */
/* MESHCORE_API_COVERAGE: meshcore_radio_rx_inject meshcore_radio_tx_done */
/* MESHCORE_API_COVERAGE: meshcore_node_advert_request */
/* MESHCORE_API_COVERAGE: meshcore_node_peer_advert_request */
/* MESHCORE_API_COVERAGE: meshcore_message_send_to_node */
/* MESHCORE_API_COVERAGE: meshcore_message_send_to_channel */
/* MESHCORE_API_COVERAGE: meshcore_channel_data_send */
/* MESHCORE_API_COVERAGE: meshcore_node_discover_path_request */
/* MESHCORE_API_COVERAGE: meshcore_node_trace_path_request */
/* MESHCORE_API_COVERAGE: meshcore_node_telemetry_request */
/* MESHCORE_API_COVERAGE: meshcore_node_binary_request */
/* MESHCORE_API_COVERAGE: meshcore_node_binary_request_with_tag */
/* MESHCORE_API_COVERAGE: meshcore_node_anon_data_send */
/* MESHCORE_API_COVERAGE: meshcore_node_binary_response */
/* MESHCORE_API_COVERAGE: meshcore_node_discover_request */
/* MESHCORE_API_COVERAGE: meshcore_raw_data_send meshcore_control_data_send */

static uint8_t s_public_key[MESHCORE_PUBLIC_KEY_SIZE];
static uint8_t s_secret[MESHCORE_CHANNEL_SECRET_MAX_LEN];
static uint8_t s_payload[MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN + 1U];
static uint8_t s_path[MESHCORE_MAX_PATH_LEN];
static uint8_t s_frame[MESHCORE_MAX_TRANS_UNIT_LEN + 1U];

static void fill_buffers(void)
{
  size_t i;

  for (i = 0U; i < sizeof(s_public_key); i++) {
    s_public_key[i] = (uint8_t)(0x10U + i);
  }
  for (i = 0U; i < sizeof(s_secret); i++) {
    s_secret[i] = (uint8_t)(0x80U + i);
  }
  for (i = 0U; i < sizeof(s_payload); i++) {
    s_payload[i] = (uint8_t)(0x40U + i);
  }
  for (i = 0U; i < sizeof(s_path); i++) {
    s_path[i] = (uint8_t)(0xC0U + i);
  }
  for (i = 0U; i < sizeof(s_frame); i++) {
    s_frame[i] = (uint8_t)(0x20U + i);
  }
}

static int init_runtime_for_api_test(void)
{
  meshcore_native_platform_reset();
  meshcore_deinit();
  return meshcore_init();
}

static int pump_until_radio_send(unsigned int previous_count)
{
  uint32_t deadline;
  uint32_t now = 0U;
  int i;

  for (i = 0; i < 16; i++) {
    if (meshcore_native_platform_radio_send_count_get() > previous_count) {
      return 0;
    }
    deadline = meshcore_native_platform_last_timer_deadline_get();
    if (deadline == 0U) {
      deadline = (uint32_t)(1U + (uint32_t)i);
    }
    if ((int32_t)(deadline - now) <= 0) {
      deadline = now + 1U;
    }
    now = deadline;
    meshcore_native_platform_time_set(now, now / 1000U);
    NATIVE_TEST_ASSERT_EQ(0, meshcore_timer_fired(now));
  }

  NATIVE_TEST_ASSERT(meshcore_native_platform_radio_send_count_get() >
                     previous_count);
  return 0;
}

static int complete_last_tx(void)
{
  uint32_t now = meshcore_native_platform_last_timer_deadline_get();

  if (now == 0U) {
    now = 1U;
  }
  now++;
  meshcore_native_platform_time_set(now, now / 1000U);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_radio_tx_done(now, true));
  return 0;
}

static int assert_runtime_entry_points_reject_uninitialized(void)
{
  meshcore_common_binary_request_event_t request = {0};
  uint32_t tag = 0U;

  NATIVE_TEST_ASSERT(meshcore_timer_fired(1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_radio_rx_inject(s_payload, 2U, -70, 4, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_radio_tx_done(1U, true) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_advert_request(false) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_peer_advert_request(s_payload, 2U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_node(s_public_key, false, 0U,
                                                   s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, 0U,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_discover_path_request(s_public_key, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_trace_path_request(s_public_key, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_telemetry_request(
      s_public_key, MESHCORE_TELEM_PERM_BASE, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request(s_public_key, s_payload, 1U) <
                     0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request_with_tag(
      s_public_key, s_payload, 1U, 0x12345678U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_anon_data_send(s_public_key, s_payload, 1U) <
                     0);
  request.route = MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  request.tag = 0x12345678U;
  NATIVE_TEST_ASSERT(meshcore_node_binary_response(&request, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_discover_request(
      MESHCORE_NODE_DISCOVER_FILTER_CHAT, false, 0U, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(s_path, 0U, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_control_data_send(s_payload, 1U) < 0);

  return 0;
}

static int test_runtime_calls_reject_before_init(void)
{
  meshcore_native_platform_reset();
  meshcore_deinit();

  return assert_runtime_entry_points_reject_uninitialized();
}

static int test_runtime_calls_reject_after_deinit(void)
{
  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  meshcore_deinit();

  return assert_runtime_entry_points_reject_uninitialized();
}

static int test_init_deinit_lifecycle_and_hook_failure(void)
{
  meshcore_native_platform_reset();
  meshcore_deinit();

  meshcore_native_platform_identity_get_fail_set(true);
  NATIVE_TEST_ASSERT(meshcore_init() < 0);
  NATIVE_TEST_ASSERT(meshcore_node_advert_request(false) < 0);

  meshcore_native_platform_reset();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_radio_begin_count_get());
  NATIVE_TEST_ASSERT(meshcore_init() < 0);

  meshcore_deinit();
  meshcore_deinit();
  NATIVE_TEST_ASSERT(meshcore_node_advert_request(false) < 0);

  meshcore_native_platform_reset();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  meshcore_deinit();
  return 0;
}

static int test_runtime_apis_reject_invalid_arguments(void)
{
  meshcore_common_binary_request_event_t request = {0};
  unsigned int radio_send_count;
  uint32_t tag = 0U;
  uint8_t control_payload = 0x80U;

  meshcore_native_platform_reset();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  radio_send_count = meshcore_native_platform_radio_send_count_get();

  NATIVE_TEST_ASSERT(meshcore_radio_rx_inject(NULL, 1U, -70, 4, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_radio_rx_inject(s_payload, 0U, -70, 4, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_radio_rx_inject(
      s_payload, MESHCORE_MAX_TRANS_UNIT_LEN + 1U, -70, 4, 1U) < 0);

  NATIVE_TEST_ASSERT(meshcore_node_peer_advert_request(NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_peer_advert_request(s_payload, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_peer_advert_request(
      s_payload, MESHCORE_MAX_RAW_ADVERT_LEN + 1U) < 0);

  NATIVE_TEST_ASSERT(meshcore_message_send_to_node(NULL, false, 0U, s_payload,
                                                   1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_node(s_public_key, false, 0U,
                                                   NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_node(s_public_key, false, 0U,
                                                   s_payload, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_node(
      s_public_key, false, 0U, s_payload, MESHCORE_MAX_MESSAGE_TX_LEN + 1U) <
                     0);

  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(
      NULL, MESHCORE_CHANNEL_SECRET_LEN_16, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(s_secret, 15U, s_payload,
                                                      1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(s_secret, 33U, s_payload,
                                                      1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, s_payload, 0U) < 0);

  NATIVE_TEST_ASSERT(meshcore_channel_data_send(
      NULL, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, 0U,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_channel_data_send(
      s_secret, 15U, NULL, 0U, MESHCORE_CHANNEL_DATA_TYPE_DEV, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, 0xC1U,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, 0U,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, s_payload,
      MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN + 1U) < 0);

  NATIVE_TEST_ASSERT(meshcore_node_discover_path_request(NULL, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_trace_path_request(NULL, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_telemetry_request(
      NULL, MESHCORE_TELEM_PERM_BASE, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_telemetry_request(s_public_key, 0x80U,
                                                     &tag) < 0);

  NATIVE_TEST_ASSERT(meshcore_node_binary_request(NULL, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request(s_public_key, NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request(s_public_key, s_payload, 0U) <
                     0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request(
      s_public_key, s_payload, MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN + 1U) <
                     0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_request_with_tag(
      s_public_key, s_payload, 0U, 0x12345678U) < 0);

  NATIVE_TEST_ASSERT(meshcore_node_anon_data_send(NULL, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_anon_data_send(s_public_key, NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_anon_data_send(s_public_key, s_payload, 0U) <
                     0);
  NATIVE_TEST_ASSERT(meshcore_node_anon_data_send(
      s_public_key, s_payload, MESHCORE_MAX_ANON_DATA_PAYLOAD_LEN + 1U) < 0);

  request.route = MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  request.tag = 0x12345678U;
  NATIVE_TEST_ASSERT(meshcore_node_binary_response(NULL, NULL, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_node_binary_response(&request, NULL,
                                                   MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN + 1U) < 0);
  request.tag = 0U;
  NATIVE_TEST_ASSERT(meshcore_node_binary_response(&request, NULL, 0U) < 0);

  NATIVE_TEST_ASSERT(meshcore_node_discover_request(0U, false, 0U, &tag) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(NULL, 1U, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(s_path, 0xC1U, s_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(s_path, 0U, NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(s_path, 0U, s_payload, 0U) < 0);
  NATIVE_TEST_ASSERT(meshcore_raw_data_send(
      s_path, 0U, s_payload, MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN + 1U) < 0);

  NATIVE_TEST_ASSERT(meshcore_control_data_send(NULL, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_control_data_send(&control_payload, 0U) < 0);
  control_payload = 0x7FU;
  NATIVE_TEST_ASSERT(meshcore_control_data_send(&control_payload, 1U) < 0);
  NATIVE_TEST_ASSERT(meshcore_control_data_send(
      s_payload, MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN + 1U) < 0);

  NATIVE_TEST_ASSERT_EQ(radio_send_count,
                        meshcore_native_platform_radio_send_count_get());
  NATIVE_TEST_ASSERT_EQ(0U,
                        meshcore_native_platform_request_error_count_get());

  meshcore_deinit();
  return 0;
}

static int test_runtime_transient_radio_failure_clears_outbound_state(void)
{
  unsigned int before_count;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  meshcore_native_platform_radio_send_fail_set(true);

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_raw_data_send(s_path, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  NATIVE_TEST_ASSERT(meshcore_native_platform_last_radio_send_len_get() > 0U);
  NATIVE_TEST_ASSERT(meshcore_radio_tx_done(2U, false) < 0);

  meshcore_native_platform_radio_send_fail_set(false);
  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_raw_data_send(s_path, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_deinit();
  return 0;
}

static int test_runtime_public_payload_limits_accept_valid_max(void)
{
  meshcore_common_binary_request_event_t response = {0};
  uint8_t control_max[MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN];
  uint32_t tag = 0x13572468U;
  size_t i;

  for (i = 0U; i < sizeof(control_max); i++) {
    control_max[i] = (uint8_t)(0x80U | (uint8_t)i);
  }

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_radio_rx_inject(
      s_frame, MESHCORE_MAX_TRANS_UNIT_LEN, -70, 4, 1U));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(
      s_public_key, true, 0U, s_payload, MESHCORE_MAX_MESSAGE_TX_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, s_payload,
      MESHCORE_MAX_MESSAGE_TX_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  meshcore_native_platform_channel_secret_match_set(true);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL,
      MESHCORE_OUT_PATH_UNKNOWN, MESHCORE_CHANNEL_DATA_TYPE_DEV, s_payload,
      MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
      s_public_key, s_payload, MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN, tag));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_anon_data_send(
      s_public_key, s_payload, MESHCORE_MAX_ANON_DATA_PAYLOAD_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  response.route = MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  memcpy(response.public_key, s_public_key, sizeof(response.public_key));
  response.tag = tag;
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_response(
      &response, s_payload, MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_raw_data_send(
      s_path, 0U, s_payload, MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN));
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_control_data_send(
      control_max, MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN));
  meshcore_deinit();

  return 0;
}

static int test_runtime_raw_and_control_success_paths_publish_frames(void)
{
  struct meshcore_packet packet;
  const uint8_t *raw;
  size_t raw_len;
  unsigned int before_count;
  uint8_t control_payload[] = {0x80U, 0x55U};

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_raw_data_send(s_path, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RAW_CUSTOM,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(1U, packet.payload_len);
  NATIVE_TEST_ASSERT_EQ(s_payload[0], packet.payload[0]);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_control_data_send(control_payload,
                                                      sizeof(control_payload)));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_CONTROL,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(sizeof(control_payload), packet.payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet.payload, control_payload,
                            sizeof(control_payload)) == 0);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_deinit();
  return 0;
}

static int test_runtime_local_advert_success_paths_publish_frames(void)
{
  struct meshcore_packet packet;
  const uint8_t *raw;
  size_t raw_len;
  unsigned int before_count;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_advert_request(false));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ADVERT,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_advert_request(true));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ADVERT,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));

  meshcore_deinit();
  return 0;
}

static int test_runtime_peer_message_route_selection(void)
{
  struct meshcore_packet packet;
  const uint8_t *raw;
  size_t raw_len;
  unsigned int before_count;
  uint8_t peer_path[] = {0x42U};

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());

  meshcore_native_platform_peer_path_set(false, false, NULL,
                                         MESHCORE_OUT_PATH_UNKNOWN, 1U);
  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, false,
                                                         0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_native_platform_peer_path_set(true, false, peer_path,
                                         sizeof(peer_path), 1U);
  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, false,
                                                         0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(1U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(peer_path[0], packet.path[0]);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_native_platform_peer_path_set(true, false, NULL, 0U, 1U);
  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, false,
                                                         0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_native_platform_peer_path_set(true, true, NULL, 0U, 1U);
  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, false,
                                                         0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, true,
                                                         0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_deinit();
  return 0;
}

static int test_runtime_channel_success_paths_publish_frames(void)
{
  struct meshcore_packet packet;
  const uint8_t *raw;
  size_t raw_len;
  unsigned int before_count;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());
  meshcore_native_platform_channel_secret_match_set(true);

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_TXT,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_channel(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_32, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_TXT,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, NULL, MESHCORE_OUT_PATH_UNKNOWN,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, NULL, 0U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_DATA,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_FLOOD,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_channel_data_send(
      s_secret, MESHCORE_CHANNEL_SECRET_LEN_16, s_path, 1U,
      MESHCORE_CHANNEL_DATA_TYPE_DEV, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_DATA,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(1U, packet.path_len);
  NATIVE_TEST_ASSERT_EQ(s_path[0], packet.path[0]);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_deinit();
  return 0;
}

static int test_runtime_node_discover_request_and_response_events(void)
{
  struct meshcore_packet packet;
  meshcore_common_node_discover_event_t const *event;
  const uint8_t *raw;
  size_t raw_len;
  unsigned int before_count;
  uint32_t tag = 0xCAFEBABEU;
  uint32_t since = 0x01020304U;
  uint8_t response_raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t response_len;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime_for_api_test());

  before_count = meshcore_native_platform_radio_send_count_get();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_discover_request(
      MESHCORE_NODE_DISCOVER_FILTER_CHAT, true, since, &tag));
  NATIVE_TEST_ASSERT_EQ(0, pump_until_radio_send(before_count));
  raw = meshcore_native_platform_last_radio_send_get();
  raw_len = meshcore_native_platform_last_radio_send_len_get();
  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&packet, raw, (uint8_t)raw_len));
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_CONTROL,
                        meshcore_packet_get_payload_type(&packet));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&packet));
  NATIVE_TEST_ASSERT_EQ(10U, packet.payload_len);
  NATIVE_TEST_ASSERT_EQ(0x81U, packet.payload[0]);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_DISCOVER_FILTER_CHAT, packet.payload[1]);
  NATIVE_TEST_ASSERT(memcmp(&packet.payload[2], &tag, sizeof(tag)) == 0);
  NATIVE_TEST_ASSERT(memcmp(&packet.payload[6], &since, sizeof(since)) == 0);
  NATIVE_TEST_ASSERT_EQ(0, complete_last_tx());

  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_CONTROL << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  packet.payload[0] = 0x91U;
  packet.payload[1] = 9U;
  memcpy(&packet.payload[2], &tag, sizeof(tag));
  memcpy(&packet.payload[6], s_public_key,
         MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES);
  packet.payload_len = 6U + MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES;
  response_len = meshcore_packet_write_to(&packet, response_raw);
  NATIVE_TEST_ASSERT(response_len > 0U);

  NATIVE_TEST_ASSERT_EQ(0, meshcore_radio_rx_inject(response_raw, response_len,
                                                   -70, 6, 123U));
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_node_discover_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_control_data_count_get());
  event = meshcore_native_platform_last_node_discover_get();
  NATIVE_TEST_ASSERT_EQ(MESHCORE_COMMON_NODE_ROLE_CHAT, event->role);
  NATIVE_TEST_ASSERT_EQ(tag, event->tag);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES,
                        event->public_key_len);
  NATIVE_TEST_ASSERT(memcmp(event->public_key, s_public_key,
                            MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES) == 0);
  NATIVE_TEST_ASSERT_EQ(9, event->uplink_snr);
  NATIVE_TEST_ASSERT_EQ(6, event->downlink_snr);

  meshcore_deinit();
  return 0;
}

int main(void)
{
  fill_buffers();

  NATIVE_TEST_ASSERT_EQ(0, test_runtime_calls_reject_before_init());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_calls_reject_after_deinit());
  NATIVE_TEST_ASSERT_EQ(0, test_init_deinit_lifecycle_and_hook_failure());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_apis_reject_invalid_arguments());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_transient_radio_failure_clears_outbound_state());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_public_payload_limits_accept_valid_max());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_raw_and_control_success_paths_publish_frames());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_local_advert_success_paths_publish_frames());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_peer_message_route_selection());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_channel_success_paths_publish_frames());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_node_discover_request_and_response_events());

  return 0;
}
