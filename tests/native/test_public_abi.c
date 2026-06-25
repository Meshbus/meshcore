// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <stddef.h>

#include "meshcore/platform.h"
#include "meshcore/runtime.h"
#include "meshcore/types.h"

/* MESHCORE_TYPE_COVERAGE: meshcore_common_node_role meshcore_common_loop_detect_mode */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_message_type meshcore_common_message_route */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_node_identity meshcore_common_node_runtime_policy */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_node_advert_profile meshcore_common_peer_identity */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_peer_path meshcore_common_channel_secret */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_identity_view meshcore_common_channel_view */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_packet_view meshcore_common_message */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_channel_data_event meshcore_common_raw_data_event */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_control_data_event meshcore_common_binary_request_event */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_node_discover_event meshcore_common_advert_event */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_peer_path_event */
/* MESHCORE_TYPE_COVERAGE: meshcore_platform_request_source meshcore_platform_telemetry_payload */

static int test_public_constants_and_roles(void)
{
  NATIVE_TEST_ASSERT_EQ(27U, MESHCORE_ABI_VERSION);
  NATIVE_TEST_ASSERT_EQ(32U, MESHCORE_PUBLIC_KEY_SIZE);
  NATIVE_TEST_ASSERT_EQ(64U, MESHCORE_PRIVATE_KEY_SIZE);
  NATIVE_TEST_ASSERT_EQ(64U, MESHCORE_MAX_PATH_LEN);
  NATIVE_TEST_ASSERT_EQ(0xFFU, MESHCORE_OUT_PATH_UNKNOWN);
  NATIVE_TEST_ASSERT_EQ(16U, MESHCORE_CHANNEL_SECRET_LEN_16);
  NATIVE_TEST_ASSERT_EQ(32U, MESHCORE_CHANNEL_SECRET_LEN_32);
  NATIVE_TEST_ASSERT_EQ(0xFFFFU, MESHCORE_CHANNEL_DATA_TYPE_DEV);

  NATIVE_TEST_ASSERT_EQ(0, MESHCORE_COMMON_NODE_ROLE_NONE);
  NATIVE_TEST_ASSERT_EQ(1, MESHCORE_COMMON_NODE_ROLE_CHAT);
  NATIVE_TEST_ASSERT_EQ(2, MESHCORE_COMMON_NODE_ROLE_REPEATER);
  NATIVE_TEST_ASSERT_EQ(3, MESHCORE_COMMON_NODE_ROLE_ROOM);
  NATIVE_TEST_ASSERT_EQ(4, MESHCORE_COMMON_NODE_ROLE_SENSOR);
  NATIVE_TEST_ASSERT_EQ(0, MESHCORE_COMMON_LOOP_DETECT_OFF);
  NATIVE_TEST_ASSERT_EQ(3, MESHCORE_COMMON_LOOP_DETECT_STRICT);
  NATIVE_TEST_ASSERT_EQ(1, MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_NODE);
  NATIVE_TEST_ASSERT_EQ(2, MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_CHANNEL);
  NATIVE_TEST_ASSERT_EQ(1, MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD);
  NATIVE_TEST_ASSERT_EQ(2, MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT);

  return 0;
}

static int test_public_view_and_config_shapes(void)
{
  meshcore_common_node_identity_t node = {0};
  meshcore_common_node_runtime_policy_t policy = {0};
  meshcore_common_node_advert_profile_t advert = {0};
  meshcore_common_peer_identity_t peer = {0};
  meshcore_common_peer_path_t peer_path = {0};
  meshcore_common_channel_secret_t channel_secret = {0};
  meshcore_common_identity_view_t identity = {0};
  meshcore_common_channel_view_t channel = {0};
  meshcore_common_packet_view_t packet = {0};
  meshcore_platform_request_source_t request_source = {0};
  meshcore_platform_telemetry_payload_t telemetry = {0};

  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_NAME_MAX_LEN, sizeof(node.name));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE, sizeof(node.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PRIVATE_KEY_SIZE, sizeof(node.private_key));
  NATIVE_TEST_ASSERT_EQ(0U, node.public_key[0]);
  NATIVE_TEST_ASSERT_EQ(0U, policy.path_hash_size);
  NATIVE_TEST_ASSERT_EQ(0U, advert.advert_interval);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_NAME_MAX_LEN, sizeof(peer.name));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE, sizeof(peer.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(peer_path.out_path));
  NATIVE_TEST_ASSERT(!peer_path.is_neighbor);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_SECRET_MAX_LEN,
                        sizeof(channel_secret.secret));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE, sizeof(identity.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_SECRET_MAX_LEN, sizeof(channel.secret));
  NATIVE_TEST_ASSERT_EQ(0U, packet.payload_type);
  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_KEY_PREFIX_BYTES,
                        sizeof(request_source.key_prefix));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_TELEMETRY_PAYLOAD_LEN,
                        sizeof(telemetry.payload));

  NATIVE_TEST_ASSERT(offsetof(meshcore_common_packet_view_t, payload) >
                     offsetof(meshcore_common_packet_view_t, payload_len));
  NATIVE_TEST_ASSERT(offsetof(meshcore_platform_telemetry_payload_t, payload) >
                     offsetof(meshcore_platform_telemetry_payload_t, payload_len));

  return 0;
}

static int test_public_event_shapes(void)
{
  meshcore_common_message_t message = {0};
  meshcore_common_channel_data_event_t channel_data = {0};
  meshcore_common_raw_data_event_t raw_data = {0};
  meshcore_common_control_data_event_t control_data = {0};
  meshcore_common_binary_request_event_t binary_request = {0};
  meshcore_common_node_discover_event_t node_discover = {0};
  meshcore_common_advert_event_t advert = {0};
  meshcore_common_peer_path_event_t peer_path = {0};

  NATIVE_TEST_ASSERT_EQ(MESHCORE_MESSAGE_TARGET_PREFIX_BYTES,
                        sizeof(message.target));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MESSAGE_SENDER_NAME_MAX_LEN,
                        sizeof(message.sender_name));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_MESSAGE_TX_LEN, sizeof(message.payload));
  NATIVE_TEST_ASSERT(offsetof(meshcore_common_message_t, payload) >
                     offsetof(meshcore_common_message_t, payload_len));

  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_HASH_BYTES,
                        sizeof(channel_data.channel_hash));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(channel_data.path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN,
                        sizeof(channel_data.payload));
  NATIVE_TEST_ASSERT(offsetof(meshcore_common_channel_data_event_t, payload) >
                     offsetof(meshcore_common_channel_data_event_t, payload_len));

  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(raw_data.path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN,
                        sizeof(raw_data.payload));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(control_data.path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN,
                        sizeof(control_data.payload));

  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_KEY_PREFIX_BYTES,
                        sizeof(binary_request.key_prefix));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE,
                        sizeof(binary_request.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(binary_request.path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN,
                        sizeof(binary_request.payload));

  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE,
                        sizeof(node_discover.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(node_discover.path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_NAME_MAX_LEN, sizeof(advert.name));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE, sizeof(advert.public_key));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(advert.out_path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_RAW_ADVERT_LEN, sizeof(advert.raw_advert));

  NATIVE_TEST_ASSERT_EQ(MESHCORE_NODE_KEY_PREFIX_BYTES,
                        sizeof(peer_path.key_prefix));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(peer_path.out_path));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(peer_path.out_path_snr));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_MAX_PATH_LEN, sizeof(peer_path.return_path_snr));

  return 0;
}

int main(void)
{
  NATIVE_TEST_ASSERT_EQ(0, test_public_constants_and_roles());
  NATIVE_TEST_ASSERT_EQ(0, test_public_view_and_config_shapes());
  NATIVE_TEST_ASSERT_EQ(0, test_public_event_shapes());

  return 0;
}
