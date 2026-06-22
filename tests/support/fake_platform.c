// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "fake_platform.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "meshcore/platform.h"

static unsigned int s_timer_arm_count;
static unsigned int s_timer_cancel_count;
static uint32_t s_last_timer_deadline;
static unsigned long s_now_ms = 1UL;
static uint32_t s_now_s = 1U;
static unsigned int s_radio_begin_count;
static unsigned int s_message_count;
static meshcore_common_message_t s_last_message;

void meshcore_native_platform_reset(void)
{
  s_timer_arm_count = 0U;
  s_timer_cancel_count = 0U;
  s_last_timer_deadline = 0U;
  s_now_ms = 1UL;
  s_now_s = 1U;
  s_radio_begin_count = 0U;
  s_message_count = 0U;
  memset(&s_last_message, 0, sizeof(s_last_message));
}

unsigned int meshcore_native_platform_timer_arm_count_get(void)
{
  return s_timer_arm_count;
}

uint32_t meshcore_native_platform_last_timer_deadline_get(void)
{
  return s_last_timer_deadline;
}

unsigned int meshcore_native_platform_message_count_get(void)
{
  return s_message_count;
}

const meshcore_common_message_t *meshcore_native_platform_last_message_get(void)
{
  return &s_last_message;
}

int meshcore_platform_timer_arm(uint32_t deadline_ms)
{
  s_timer_arm_count++;
  s_last_timer_deadline = deadline_ms;
  return 0;
}

void meshcore_platform_timer_cancel(void)
{
  s_timer_cancel_count++;
}

unsigned long meshcore_platform_millis_get(void)
{
  return s_now_ms;
}

uint32_t meshcore_platform_rtc_get_current_time(void)
{
  return s_now_s;
}

void meshcore_platform_radio_begin(void)
{
  s_radio_begin_count++;
}

int meshcore_platform_radio_packet_send(const uint8_t *data, size_t len)
{
  return (data != NULL && len > 0U) ? 1 : 0;
}

uint32_t meshcore_platform_radio_airtime(size_t len)
{
  return (uint32_t)(len + 1U);
}

float meshcore_platform_radio_packet_score(int8_t snr_q4, size_t len)
{
  (void)len;
  return ((float)snr_q4) / 4.0f;
}

bool meshcore_platform_radio_in_rx_mode_get(void)
{
  return true;
}

bool meshcore_platform_radio_receiving_get(void)
{
  return false;
}

void meshcore_platform_radio_noise_floor_calibrate(int threshold)
{
  (void)threshold;
}

void meshcore_platform_radio_agc_reset(void)
{
}

void meshcore_platform_rng_random(uint8_t *dest, size_t size)
{
  size_t i;

  if (dest == NULL) {
    return;
  }
  for (i = 0U; i < size; i++) {
    dest[i] = (uint8_t)(0xA5U + i);
  }
}

static void fake_digest(uint8_t *dest, size_t dest_len, const uint8_t *data,
                        size_t data_len)
{
  size_t i;
  uint8_t acc = 0x5AU;

  if (dest == NULL) {
    return;
  }
  for (i = 0U; i < data_len; i++) {
    acc = (uint8_t)(acc + data[i] + (uint8_t)i);
  }
  for (i = 0U; i < dest_len; i++) {
    dest[i] = (uint8_t)(acc + i);
  }
}

bool meshcore_platform_crypto_sha256(uint8_t *hash, size_t hash_len,
                                     const uint8_t *msg, int msg_len)
{
  if (hash == NULL || msg_len < 0 || (msg == NULL && msg_len > 0)) {
    return false;
  }
  fake_digest(hash, hash_len, msg, (size_t)msg_len);
  return true;
}

bool meshcore_platform_crypto_sha256_two_fragments(
    uint8_t *hash, size_t hash_len, const uint8_t *frag1, int frag1_len,
    const uint8_t *frag2, int frag2_len)
{
  uint8_t buf[128];
  size_t len = 0U;

  if (hash == NULL || frag1_len < 0 || frag2_len < 0 ||
      (frag1 == NULL && frag1_len > 0) || (frag2 == NULL && frag2_len > 0) ||
      (size_t)frag1_len + (size_t)frag2_len > sizeof(buf)) {
    return false;
  }
  if (frag1_len > 0) {
    memcpy(&buf[len], frag1, (size_t)frag1_len);
    len += (size_t)frag1_len;
  }
  if (frag2_len > 0) {
    memcpy(&buf[len], frag2, (size_t)frag2_len);
    len += (size_t)frag2_len;
  }
  fake_digest(hash, hash_len, buf, len);
  return true;
}

bool meshcore_platform_crypto_aes128_encrypt_block(const uint8_t *key,
                                                   uint8_t *dest,
                                                   const uint8_t *src)
{
  (void)key;
  if (dest == NULL || src == NULL) {
    return false;
  }
  memcpy(dest, src, 16U);
  return true;
}

bool meshcore_platform_crypto_aes128_decrypt_block(const uint8_t *key,
                                                   uint8_t *dest,
                                                   const uint8_t *src)
{
  return meshcore_platform_crypto_aes128_encrypt_block(key, dest, src);
}

bool meshcore_platform_crypto_hmac_sha256(uint8_t *mac, size_t mac_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *msg, size_t msg_len)
{
  (void)key;
  (void)key_len;
  if (mac == NULL || (msg == NULL && msg_len > 0U)) {
    return false;
  }
  fake_digest(mac, mac_len, msg, msg_len);
  return true;
}

int meshcore_platform_node_identity_get(meshcore_common_node_identity_t *out)
{
  size_t i;

  if (out == NULL) {
    return -EINVAL;
  }
  memset(out, 0, sizeof(*out));
  out->role = MESHCORE_COMMON_NODE_ROLE_CHAT;
  for (i = 0U; i < sizeof(out->public_key); i++) {
    out->public_key[i] = (uint8_t)(0x10U + i);
  }
  for (i = 0U; i < sizeof(out->private_key); i++) {
    out->private_key[i] = (uint8_t)(0x80U + i);
  }
  return 0;
}

int meshcore_platform_node_config_last_modify_get(uint32_t *out_timestamp)
{
  if (out_timestamp == NULL) {
    return -EINVAL;
  }
  *out_timestamp = 1U;
  return 0;
}

int meshcore_platform_node_runtime_policy_get(
    meshcore_common_node_runtime_policy_t *out)
{
  if (out == NULL) {
    return -EINVAL;
  }
  memset(out, 0, sizeof(*out));
  out->path_hash_size = 1U;
  out->tx_delay_factor = 0.5f;
  out->direct_tx_delay_factor = 0.2f;
  return 0;
}

int meshcore_platform_node_advert_profile_get(
    meshcore_common_node_advert_profile_t *out)
{
  if (out == NULL) {
    return -EINVAL;
  }
  memset(out, 0, sizeof(*out));
  out->advert_interval = 10000U;
  out->flood_advert_interval = 60000U;
  return 0;
}

int meshcore_platform_peer_path_get_by_key(
    const uint8_t *public_key, meshcore_common_peer_path_t *out)
{
  (void)public_key;
  if (out == NULL) {
    return -EINVAL;
  }
  memset(out, 0, sizeof(*out));
  out->out_path_len = MESHCORE_OUT_PATH_UNKNOWN;
  out->path_hash_size = 1U;
  return -ENOENT;
}

int meshcore_platform_peer_seen_update(const uint8_t *public_key, bool has_snr,
                                       int8_t snr_q4)
{
  (void)public_key;
  (void)has_snr;
  (void)snr_q4;
  return 0;
}

int meshcore_platform_peer_next_shared_secret_by_hash(
    const uint8_t *hash, size_t start_slot, size_t *slot_id,
    uint8_t *dest_secret, meshcore_common_peer_identity_t *peer_identity)
{
  (void)hash;
  (void)start_slot;
  if (slot_id == NULL || dest_secret == NULL || peer_identity == NULL) {
    return -EINVAL;
  }
  memset(dest_secret, 0, MESHCORE_CHANNEL_SECRET_MAX_LEN);
  memset(peer_identity, 0, sizeof(*peer_identity));
  return -ENOENT;
}

int meshcore_platform_channel_secret_match_exists(uint8_t channel_hash,
                                                  const uint8_t *secret,
                                                  size_t secret_len)
{
  (void)channel_hash;
  (void)secret;
  (void)secret_len;
  return 0;
}

int meshcore_platform_channel_secret_hash(const uint8_t *secret,
                                          size_t secret_len,
                                          uint8_t *out_hash)
{
  if (secret == NULL || secret_len == 0U || out_hash == NULL) {
    return -EINVAL;
  }
  out_hash[0] = secret[0];
  return 0;
}

int meshcore_platform_channel_search_by_hash(
    const uint8_t *hash, meshcore_common_channel_view_t *channels,
    int max_matches)
{
  (void)hash;
  (void)channels;
  (void)max_matches;
  return 0;
}

void meshcore_platform_dispatcher_log_rx_raw(float snr, float rssi,
                                             const uint8_t raw[], int len)
{
  (void)snr;
  (void)rssi;
  (void)raw;
  (void)len;
}

void meshcore_platform_dispatcher_log_rx(
    const meshcore_common_packet_view_t *packet, int len, float score)
{
  (void)packet;
  (void)len;
  (void)score;
}

void meshcore_platform_dispatcher_log_tx(
    const meshcore_common_packet_view_t *packet, int len)
{
  (void)packet;
  (void)len;
}

void meshcore_platform_dispatcher_log_tx_fail(
    const meshcore_common_packet_view_t *packet, int len)
{
  (void)packet;
  (void)len;
}

float meshcore_platform_dispatcher_airtime_budget_factor_get(void)
{
  return 1.0f;
}

int meshcore_platform_dispatcher_rx_delay_calc(float score, uint32_t air_time)
{
  (void)score;
  return (int)air_time;
}

uint32_t meshcore_platform_dispatcher_cad_fail_max_duration_get(void)
{
  return 4000U;
}

int meshcore_platform_dispatcher_interference_threshold_get(void)
{
  return 0;
}

int meshcore_platform_dispatcher_agc_reset_interval_get(void)
{
  return 0;
}

unsigned long meshcore_platform_dispatcher_duty_cycle_window_ms_get(void)
{
  return 3600000UL;
}

uint32_t meshcore_platform_mesh_cad_fail_retry_delay_get(void)
{
  return 120U;
}

bool meshcore_platform_mesh_filter_recv_flood_packet(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
  return false;
}

bool meshcore_platform_mesh_allow_packet_forward(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
  return false;
}

uint32_t meshcore_platform_mesh_retransmit_delay_get(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
  return 0U;
}

uint32_t meshcore_platform_mesh_direct_retransmit_delay_get(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
  return 0U;
}

uint8_t meshcore_platform_mesh_extra_ack_transmit_count_get(void)
{
  return 0U;
}

void meshcore_platform_mesh_on_peer_data_recv(
    const meshcore_common_packet_view_t *packet, uint8_t type,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *data, size_t len)
{
  (void)packet;
  (void)type;
  (void)sender;
  (void)secret;
  (void)data;
  (void)len;
}

void meshcore_platform_mesh_on_trace_recv(
    const meshcore_common_packet_view_t *packet, uint32_t tag,
    uint32_t auth_code, uint8_t flags, const uint8_t *path_snrs,
    const uint8_t *path_hashes, uint8_t path_len)
{
  (void)packet;
  (void)tag;
  (void)auth_code;
  (void)flags;
  (void)path_snrs;
  (void)path_hashes;
  (void)path_len;
}

bool meshcore_platform_mesh_on_peer_path_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len)
{
  (void)packet;
  (void)sender;
  (void)secret;
  (void)path;
  (void)path_len;
  (void)extra_type;
  (void)extra;
  (void)extra_len;
  return false;
}

void meshcore_platform_mesh_on_advert_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_identity_view_t *identity, uint32_t timestamp,
    const uint8_t *app_data, size_t app_data_len)
{
  (void)packet;
  (void)identity;
  (void)timestamp;
  (void)app_data;
  (void)app_data_len;
}

void meshcore_platform_mesh_on_anon_data_recv(
    const meshcore_common_packet_view_t *packet, const uint8_t *secret,
    const meshcore_common_identity_view_t *sender, uint8_t *data, size_t len)
{
  (void)packet;
  (void)secret;
  (void)sender;
  (void)data;
  (void)len;
}

void meshcore_platform_mesh_on_path_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_identity_view_t *sender, uint8_t *path,
    uint8_t path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len)
{
  (void)packet;
  (void)sender;
  (void)path;
  (void)path_len;
  (void)extra_type;
  (void)extra;
  (void)extra_len;
}

void meshcore_platform_mesh_on_control_data_recv(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
}

void meshcore_platform_mesh_on_raw_data_recv(
    const meshcore_common_packet_view_t *packet)
{
  (void)packet;
}

void meshcore_platform_mesh_on_group_data_recv(
    const meshcore_common_packet_view_t *packet, uint8_t type,
    const meshcore_common_channel_view_t *channel, uint8_t *data, size_t len)
{
  (void)packet;
  (void)type;
  (void)channel;
  (void)data;
  (void)len;
}

void meshcore_platform_mesh_on_ack_recv(
    const meshcore_common_packet_view_t *packet, uint32_t ack_crc)
{
  (void)packet;
  (void)ack_crc;
}

void meshcore_platform_runtime_request_error(uint8_t request_type,
                                             int err_code)
{
  (void)request_type;
  (void)err_code;
}

int meshcore_platform_event_message(const meshcore_common_message_t *message)
{
  if (message != NULL) {
    s_last_message = *message;
    s_message_count++;
  }
  return 0;
}

int meshcore_platform_event_message_ack(const uint8_t *target, uint8_t attempt)
{
  (void)target;
  (void)attempt;
  return 0;
}

int meshcore_platform_event_advert(const meshcore_common_advert_event_t *advert)
{
  (void)advert;
  return 0;
}

int meshcore_platform_event_peer_path_publish(
    const meshcore_common_peer_path_event_t *peer_path, bool is_discover)
{
  (void)peer_path;
  (void)is_discover;
  return 0;
}

int meshcore_platform_event_peer_path(
    const meshcore_common_peer_path_event_t *peer_path)
{
  (void)peer_path;
  return 0;
}

int meshcore_platform_event_trace_path(const uint8_t *key_prefix,
                                       uint8_t state,
                                       uint32_t tag,
                                       const int8_t *out_path_snr,
                                       uint8_t out_count,
                                       const int8_t *return_path_snr,
                                       uint8_t return_count,
                                       bool has_response_snr,
                                       int8_t response_snr,
                                       uint32_t timestamp)
{
  (void)key_prefix;
  (void)state;
  (void)tag;
  (void)out_path_snr;
  (void)out_count;
  (void)return_path_snr;
  (void)return_count;
  (void)has_response_snr;
  (void)response_snr;
  (void)timestamp;
  return 0;
}

int meshcore_platform_event_telemetry(const uint8_t *key_prefix,
                                      uint32_t timestamp,
                                      uint32_t tag,
                                      const uint8_t *payload,
                                      size_t payload_len)
{
  (void)key_prefix;
  (void)timestamp;
  (void)tag;
  (void)payload;
  (void)payload_len;
  return 0;
}

int meshcore_platform_event_binary_request(
    const meshcore_common_binary_request_event_t *event)
{
  (void)event;
  return 0;
}

int meshcore_platform_event_binary_response(const uint8_t *key_prefix,
                                            uint32_t timestamp, uint32_t tag,
                                            const uint8_t *payload,
                                            size_t payload_len)
{
  (void)key_prefix;
  (void)timestamp;
  (void)tag;
  (void)payload;
  (void)payload_len;
  return 0;
}

int meshcore_platform_event_node_discover(
    const meshcore_common_node_discover_event_t *event)
{
  (void)event;
  return 0;
}

int meshcore_platform_event_channel_data(
    const meshcore_common_channel_data_event_t *event)
{
  (void)event;
  return 0;
}

int meshcore_platform_event_raw_data(
    const meshcore_common_raw_data_event_t *event)
{
  (void)event;
  return 0;
}

int meshcore_platform_event_control_data(
    const meshcore_common_control_data_event_t *event)
{
  (void)event;
  return 0;
}

int meshcore_platform_telemetry_node_get(
    const meshcore_platform_request_source_t *requester,
    uint8_t permission_mask, meshcore_platform_telemetry_payload_t *out)
{
  (void)requester;
  (void)permission_mask;
  if (out == NULL) {
    return -EINVAL;
  }
  memset(out, 0, sizeof(*out));
  return 0;
}
