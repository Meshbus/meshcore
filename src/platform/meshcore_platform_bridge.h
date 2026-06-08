// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_PLATFORM_BRIDGE_H_
#define MESHCORE_PLATFORM_BRIDGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/platform.h"

struct meshcore_packet;
struct meshcore_identity;
struct meshcore_group_channel;

void meshcore_platform_bridge_radio_begin(void);
int meshcore_platform_bridge_radio_packet_send(const uint8_t *data,
                                                size_t len);
uint32_t meshcore_platform_bridge_radio_airtime(size_t len);
float meshcore_platform_bridge_radio_packet_score(int8_t snr_q4, size_t len);
bool meshcore_platform_bridge_radio_in_rx_mode_get(void);
bool meshcore_platform_bridge_radio_receiving_get(void);
void meshcore_platform_bridge_radio_noise_floor_calibrate(int threshold);
void meshcore_platform_bridge_radio_agc_reset(void);
void meshcore_platform_bridge_rng_random(uint8_t *dest, size_t size);
unsigned long meshcore_platform_bridge_millis_get(void);
uint32_t meshcore_platform_bridge_rtc_get_current_time(void);
int meshcore_platform_bridge_telemetry_node_get(
    const meshcore_platform_request_source_t *requester,
    uint8_t permission_mask, meshcore_platform_telemetry_payload_t *out);
bool meshcore_platform_bridge_crypto_sha256(uint8_t *hash, size_t hash_len,
                                      const uint8_t *msg, int msg_len);
bool meshcore_platform_bridge_crypto_sha256_two_fragments(
    uint8_t *hash, size_t hash_len, const uint8_t *frag1, int frag1_len,
    const uint8_t *frag2, int frag2_len);
bool meshcore_platform_bridge_crypto_aes128_encrypt_block(const uint8_t *key,
                                                    uint8_t *dest,
                                                    const uint8_t *src);
bool meshcore_platform_bridge_crypto_aes128_decrypt_block(const uint8_t *key,
                                                    uint8_t *dest,
                                                    const uint8_t *src);
bool meshcore_platform_bridge_crypto_hmac_sha256(uint8_t *mac, size_t mac_len,
                                           const uint8_t *key, size_t key_len,
                                           const uint8_t *msg, size_t msg_len);

int meshcore_platform_bridge_node_identity_get(
    meshcore_common_node_identity_t *out);
int meshcore_platform_bridge_node_config_last_modify_get(
    uint32_t *out_timestamp);
int meshcore_platform_bridge_node_runtime_policy_get(
    meshcore_common_node_runtime_policy_t *out);
int meshcore_platform_bridge_node_advert_profile_get(
    meshcore_common_node_advert_profile_t *out);
int meshcore_platform_bridge_peer_path_get_by_key(
    const uint8_t *public_key, meshcore_common_peer_path_t *out);
int meshcore_platform_bridge_peer_seen_update(const uint8_t *public_key,
                                           bool has_snr, int8_t snr_q4);
int meshcore_platform_bridge_channel_secret_match_exists(uint8_t channel_hash,
                                                      const uint8_t *secret,
                                                      size_t secret_len);
int meshcore_platform_bridge_channel_secret_hash(const uint8_t *secret,
                                              size_t secret_len,
                                              uint8_t *out_hash);
void meshcore_platform_bridge_request_error(uint8_t request_type, int err_code);
uint8_t meshcore_platform_bridge_mesh_extra_ack_transmit_count_get(void);
uint32_t meshcore_platform_bridge_mesh_retransmit_delay_get(
    const struct meshcore_packet *packet);
int meshcore_platform_bridge_message_handler(
    const meshcore_common_message_t *message);
int meshcore_platform_bridge_message_ack_handler(const uint8_t *target,
                                              uint8_t attempt);
int meshcore_platform_bridge_advert_handler(
    const meshcore_common_advert_event_t *advert);
int meshcore_platform_bridge_peer_path_publish(
    const meshcore_common_peer_path_event_t *peer_path, bool is_discover);
int meshcore_platform_bridge_peer_path_handler(
    const meshcore_common_peer_path_event_t *peer_path);
int meshcore_platform_bridge_trace_path_handler(
    const uint8_t *key_prefix, uint8_t state, uint32_t tag, const int8_t *out_path_snr,
    uint8_t out_count, const int8_t *return_path_snr, uint8_t return_count,
    bool has_response_snr, int8_t response_snr, uint32_t timestamp);
int meshcore_platform_bridge_telemetry_handler(const uint8_t *key_prefix,
                                            uint32_t timestamp,
                                            uint32_t tag,
                                            const uint8_t *payload,
                                            size_t payload_len);
int meshcore_platform_bridge_binary_response_handler(const uint8_t *key_prefix,
                                                  uint32_t timestamp,
                                                  uint32_t tag,
                                                  const uint8_t *payload,
                                                  size_t payload_len);
int meshcore_platform_bridge_channel_data_handler(
    const meshcore_common_channel_data_event_t *event);
int meshcore_platform_bridge_raw_data_handler(
    const meshcore_common_raw_data_event_t *event);
int meshcore_platform_bridge_control_data_handler(
    const meshcore_common_control_data_event_t *event);

void meshcore_platform_bridge_dispatcher_log_rx_raw(float snr, float rssi,
                                                 const uint8_t raw[], int len);
void meshcore_platform_bridge_dispatcher_log_rx(
    const struct meshcore_packet *packet, int len, float score);
void meshcore_platform_bridge_dispatcher_log_tx(
    const struct meshcore_packet *packet, int len);
void meshcore_platform_bridge_dispatcher_log_tx_fail(
    const struct meshcore_packet *packet, int len);
float meshcore_platform_bridge_dispatcher_airtime_budget_factor_get(void);
int meshcore_platform_bridge_dispatcher_rx_delay_calc(float score,
                                                   uint32_t air_time);
uint32_t meshcore_platform_bridge_dispatcher_cad_fail_max_duration_get(void);
int meshcore_platform_bridge_dispatcher_interference_threshold_get(void);
int meshcore_platform_bridge_dispatcher_agc_reset_interval_get(void);
unsigned long meshcore_platform_bridge_dispatcher_duty_cycle_window_ms_get(void);

uint32_t meshcore_platform_bridge_mesh_cad_fail_retry_delay_get(void);
bool meshcore_platform_bridge_mesh_filter_recv_flood_packet(
    const struct meshcore_packet *packet);
bool meshcore_platform_bridge_mesh_allow_packet_forward(
    const struct meshcore_packet *packet);
uint32_t meshcore_platform_bridge_mesh_direct_retransmit_delay_get(
    const struct meshcore_packet *packet);
int meshcore_platform_bridge_mesh_next_peer_shared_secret_by_hash(
    const uint8_t *hash, size_t start_slot, size_t *slot_id,
    uint8_t *dest_secret, meshcore_common_peer_identity_t *peer_identity);
int meshcore_platform_bridge_mesh_search_channels_by_hash(
    const uint8_t *hash, struct meshcore_group_channel *channels,
    int max_matches);
void meshcore_platform_bridge_mesh_on_peer_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *data, size_t len);
void meshcore_platform_bridge_mesh_on_trace_recv(
    struct meshcore_packet *packet, uint32_t tag, uint32_t auth_code,
    uint8_t flags, const uint8_t *path_snrs, const uint8_t *path_hashes,
    uint8_t path_len);
bool meshcore_platform_bridge_mesh_on_peer_path_recv(
    struct meshcore_packet *packet,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len);
void meshcore_platform_bridge_mesh_on_advert_recv(
    struct meshcore_packet *packet, const struct meshcore_identity *identity,
    uint32_t timestamp, const uint8_t *app_data, size_t app_data_len);
void meshcore_platform_bridge_mesh_on_anon_data_recv(
    struct meshcore_packet *packet, const uint8_t *secret,
    const struct meshcore_identity *sender, uint8_t *data, size_t len);
void meshcore_platform_bridge_mesh_on_path_recv(struct meshcore_packet *packet,
                                             struct meshcore_identity *sender,
                                             uint8_t *path, uint8_t path_len,
                                             uint8_t extra_type,
                                             uint8_t *extra,
                                             uint8_t extra_len);
void meshcore_platform_bridge_mesh_on_control_data_recv(
    struct meshcore_packet *packet);
void meshcore_platform_bridge_mesh_on_raw_data_recv(struct meshcore_packet *packet);
void meshcore_platform_bridge_mesh_on_group_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data, size_t len);
void meshcore_platform_bridge_mesh_on_ack_recv(struct meshcore_packet *packet,
                                            uint32_t ack_crc);

#endif /* MESHCORE_PLATFORM_BRIDGE_H_ */
