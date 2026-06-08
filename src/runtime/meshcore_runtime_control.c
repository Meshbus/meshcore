// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <string.h>

#include "meshcore_platform_bridge.h"

bool meshcore_runtime_handle_control_discover_request(
    struct meshcore_packet *packet) {
  meshcore_common_node_identity_t identity;
  struct meshcore_packet *reply;
  uint8_t data[6U + MESHCORE_PUBLIC_KEY_SIZE];
  uint32_t last_modify = 0U;
  uint32_t tag;
  uint32_t since = 0U;
  uint8_t filter;
  bool prefix_only;

  if (packet == NULL || packet->payload_len < 6U ||
      !meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER) ||
      (packet->payload[0] & 0xF0U) != MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_REQ ||
      meshcore_platform_bridge_node_config_last_modify_get(&last_modify) != 0 ||
      !meshcore_runtime_local_identity_get(&identity)) {
    return false;
  }

  filter = packet->payload[1];
  memcpy(&tag, &packet->payload[2], sizeof(tag));
  if (packet->payload_len >= 10U) {
    memcpy(&since, &packet->payload[6], sizeof(since));
  }
  if ((filter & (1U << ADV_TYPE_REPEATER)) == 0U || last_modify < since) {
    return false;
  }

  prefix_only = (packet->payload[0] & 0x01U) != 0U;
  data[0] = MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER;
  data[1] = (uint8_t)(packet->snr_q4 & 0xFFU);
  memcpy(&data[2], &tag, sizeof(tag));
  memcpy(&data[6], identity.public_key, sizeof(identity.public_key));

  reply = meshcore_mesh_create_control_data(
      &meshcore_runtime_context_get()->mesh, data,
      prefix_only ? (size_t)(6U + 8U) : (size_t)(6U + sizeof(identity.public_key)));
  if (reply == NULL) {
    return false;
  }

  meshcore_mesh_send_zero_hop(
      &meshcore_runtime_context_get()->mesh, reply,
      meshcore_mesh_runtime_get_retransmit_delay(
          &meshcore_runtime_context_get()->mesh, reply) * 4U);
  return true;
}
