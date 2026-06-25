// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include "meshcore_platform_bridge.h"
#include "meshcore_rng.h"

static const uint8_t loop_detect_max_minimal[] = {0U, 4U, 2U, 1U};
static const uint8_t loop_detect_max_moderate[] = {0U, 2U, 1U, 1U};
static const uint8_t loop_detect_max_strict[] = {0U, 1U, 1U, 1U};

static bool meshcore_runtime_policy_get(
    meshcore_common_node_runtime_policy_t *out) {
  return out != NULL &&
         meshcore_platform_bridge_node_runtime_policy_get(out) == 0;
}

static bool meshcore_runtime_packet_is_looped(
    const struct meshcore_packet *packet, const uint8_t max_counters[]) {
  uint8_t hash_size;
  uint8_t stride;
  uint8_t hash_count;
  uint8_t self_count = 0U;
  const uint8_t *path;

  if (packet == NULL || max_counters == NULL ||
      meshcore_runtime_sync_local_identity() != 0) {
    return false;
  }

  stride = meshcore_packet_get_path_hash_size(packet);
  hash_size = stride;
  if (meshcore_packet_has_snr_flag(packet)) {
    if (stride < 2U) {
      return false;
    }
    hash_size = (uint8_t)(stride - 1U);
  }
  hash_count = meshcore_packet_get_path_hash_count(packet);
  path = packet->path;

  if (hash_size == 0U || hash_size > 3U || stride == 0U || stride > 3U) {
    return false;
  }

  while (hash_count > 0U) {
    if (meshcore_identity_is_hash_match_by_len(
            &meshcore_runtime_context_get()->mesh.self_id.identity, path,
            hash_size)) {
      self_count++;
    }
    hash_count--;
    path += stride;
  }

  return self_count >= max_counters[hash_size];
}

static bool meshcore_runtime_client_repeat_active(
    const meshcore_common_node_runtime_policy_t *policy) {
  return policy != NULL && policy->client_repeat &&
         meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT);
}

bool meshcore_runtime_protocol_allow_packet_forward(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet) {
  meshcore_common_node_runtime_policy_t policy;
  const uint8_t *maximums = loop_detect_max_strict;

  (void)user_data;
  (void)mesh;

  if (packet == NULL || !meshcore_runtime_policy_get(&policy)) {
    return false;
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER)) {
    if (policy.disable_fwd) {
      return false;
    }

    if (meshcore_packet_is_route_flood(packet)) {
      if (meshcore_packet_get_path_hash_count(packet) >= policy.flood_max) {
        return false;
      }

      if (policy.loop_detect != MESHCORE_COMMON_LOOP_DETECT_OFF) {
        if (policy.loop_detect == MESHCORE_COMMON_LOOP_DETECT_MINIMAL) {
          maximums = loop_detect_max_minimal;
        } else if (policy.loop_detect == MESHCORE_COMMON_LOOP_DETECT_MODERATE) {
          maximums = loop_detect_max_moderate;
        }

        if (meshcore_runtime_packet_is_looped(packet, maximums)) {
          return false;
        }
      }
    }

    return true;
  }

  return meshcore_runtime_client_repeat_active(&policy);
}

static uint32_t meshcore_runtime_packet_airtime(
    const struct meshcore_packet *packet) {
  int raw_len;

  if (packet == NULL) {
    return 0U;
  }

  raw_len = meshcore_packet_get_raw_length(packet);
  if (raw_len <= 0) {
    return 0U;
  }

  return meshcore_platform_bridge_radio_airtime((size_t)raw_len);
}

static uint32_t meshcore_runtime_randomized_delay(uint32_t airtime,
                                                  float factor) {
  uint32_t t = (uint32_t)((float)airtime * factor);

  return meshcore_rng_next_int(0U, 5U * t + 1U);
}

uint32_t meshcore_runtime_protocol_get_retransmit_delay(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet) {
  meshcore_common_node_runtime_policy_t policy;
  uint32_t airtime;
  uint32_t t;

  (void)user_data;
  (void)mesh;

  if (packet == NULL || !meshcore_runtime_policy_get(&policy)) {
    return 0U;
  }

  airtime = meshcore_runtime_packet_airtime(packet);
  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER)) {
    return meshcore_runtime_randomized_delay(airtime, policy.tx_delay_factor);
  }

  if (meshcore_runtime_client_repeat_active(&policy)) {
    return meshcore_runtime_randomized_delay(airtime, 0.5f);
  }

  t = (airtime * 52U / 50U) / 2U;
  return meshcore_rng_next_int(0U, 5U) * t;
}

uint32_t meshcore_runtime_protocol_get_direct_retransmit_delay(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet) {
  meshcore_common_node_runtime_policy_t policy;
  uint32_t airtime;

  (void)user_data;
  (void)mesh;

  if (packet == NULL || !meshcore_runtime_policy_get(&policy)) {
    return 0U;
  }

  airtime = meshcore_runtime_packet_airtime(packet);
  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER)) {
    return meshcore_runtime_randomized_delay(airtime,
                                             policy.direct_tx_delay_factor);
  }

  if (meshcore_runtime_client_repeat_active(&policy)) {
    return meshcore_runtime_randomized_delay(airtime, 0.2f);
  }

  return 0U;
}
