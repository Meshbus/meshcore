/*
 * Copyright (c) 2026 FoBE Studio
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Host-implemented MeshCore platform hooks.
 */

#ifndef MESHCORE_PLATFORM_H_
#define MESHCORE_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup meshcore_platform MeshCore Platform Hooks
 * @brief Link-time singleton hooks implemented by the host platform.
 * @ingroup meshcore
 *
 * This header is the implementation side of the public ABI. A host that links
 * the full runtime must provide exactly one definition for every
 * meshcore_platform_*() function declared here. The runtime binds these hooks
 * directly at link time; there are no weak fallbacks and no runtime-installed
 * function table.
 *
 * Hooks marked optional still require a concrete symbol. Unsupported optional
 * behavior should be an explicit stub returning a negative errno-style value,
 * false, 0, or no-op according to the hook documentation.
 *
 * The callable library API is declared by @ref meshcore_runtime.
 *
 * @{
 */

/**
 * @brief Packet-derived requester identity available to host services.
 */
typedef struct meshcore_platform_request_source {
  /** Requester public-key prefix. */
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
} meshcore_platform_request_source_t;

/**
 * @brief Telemetry payload exchanged between MeshCore and the host.
 */
typedef struct meshcore_platform_telemetry_payload {
  /** True when @ref latitude is valid. */
  bool has_latitude;
  /** Latitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t latitude;
  /** True when @ref longitude is valid. */
  bool has_longitude;
  /** Longitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t longitude;
  /** Number of valid bytes in @ref payload. */
  uint8_t payload_len;
  /** Encoded telemetry payload bytes. */
  uint8_t payload[MESHCORE_MAX_TELEMETRY_PAYLOAD_LEN];
} meshcore_platform_telemetry_payload_t;

/**
 * @name Runtime Scheduling Hooks
 * @{
 */

/**
 * @brief Arm the host timer used by the runtime.
 *
 * @param deadline_ms Absolute host uptime deadline in milliseconds.
 * @return 0 on success, or a negative errno-style value on failure.
 */
int meshcore_platform_timer_arm(uint32_t deadline_ms);

/**
 * @brief Cancel the host timer used by the runtime.
 */
void meshcore_platform_timer_cancel(void);

/** @} */

/**
 * @name Host Time Sources
 * @{
 */

/**
 * @brief Get monotonic host uptime in milliseconds.
 *
 * @return Monotonic uptime in milliseconds.
 */
unsigned long meshcore_platform_millis_get(void);

/**
 * @brief Get wall-clock time used in MeshCore packet timestamps.
 *
 * @return Current host time in seconds.
 */
uint32_t meshcore_platform_rtc_get_current_time(void);

/** @} */

/**
 * @name Radio Hooks
 * @{
 */

/**
 * @brief Initialize or resume the host radio backend.
 */
void meshcore_platform_radio_begin(void);

/**
 * @brief Send a serialized MeshCore radio frame.
 *
 * @param data Serialized frame bytes.
 * @param len Number of bytes in @p data.
 * @return Nonzero when the frame was accepted for transmission, otherwise 0.
 */
int meshcore_platform_radio_packet_send(const uint8_t *data, size_t len);

/**
 * @brief Estimate radio airtime for a serialized frame length.
 *
 * @param len Serialized frame length in bytes.
 * @return Estimated airtime in milliseconds.
 */
uint32_t meshcore_platform_radio_airtime(size_t len);

/**
 * @brief Score a received frame for dispatcher timing decisions.
 *
 * @param snr_q4 SNR in quarter-dB units.
 * @param len Serialized frame length in bytes.
 * @return Host-defined score; larger values should indicate better quality.
 */
float meshcore_platform_radio_packet_score(int8_t snr_q4, size_t len);

/**
 * @brief Check whether the radio is currently in receive mode.
 *
 * @return true if the radio is in receive mode.
 */
bool meshcore_platform_radio_in_rx_mode_get(void);

/**
 * @brief Check whether the radio is currently receiving a frame.
 *
 * @return true if the radio is mid-receive or channel activity should defer TX.
 */
bool meshcore_platform_radio_receiving_get(void);

/**
 * @brief Ask the host radio backend to calibrate its noise floor.
 *
 * @param threshold Host-defined activity threshold.
 */
void meshcore_platform_radio_noise_floor_calibrate(int threshold);

/**
 * @brief Reset the host radio automatic gain control state.
 */
void meshcore_platform_radio_agc_reset(void);

/** @} */

/**
 * @name Random And Cryptographic Hooks
 * @{
 */

/**
 * @brief Fill a buffer with random bytes.
 *
 * Production hosts should provide cryptographically suitable entropy.
 *
 * @param dest Destination buffer.
 * @param size Number of bytes to write.
 */
void meshcore_platform_rng_random(uint8_t *dest, size_t size);

/**
 * @brief Compute SHA-256 for one message fragment.
 *
 * @param hash Destination hash buffer.
 * @param hash_len Size of @p hash in bytes.
 * @param msg Message bytes, or NULL when @p msg_len is 0.
 * @param msg_len Number of bytes in @p msg.
 * @return true on success, false on failure.
 */
bool meshcore_platform_crypto_sha256(uint8_t *hash, size_t hash_len,
                                     const uint8_t *msg, int msg_len);

/**
 * @brief Compute SHA-256 for two contiguous message fragments.
 *
 * @param hash Destination hash buffer.
 * @param hash_len Size of @p hash in bytes.
 * @param frag1 First fragment, or NULL when @p frag1_len is 0.
 * @param frag1_len Number of bytes in @p frag1.
 * @param frag2 Second fragment, or NULL when @p frag2_len is 0.
 * @param frag2_len Number of bytes in @p frag2.
 * @return true on success, false on failure.
 */
bool meshcore_platform_crypto_sha256_two_fragments(
    uint8_t *hash, size_t hash_len, const uint8_t *frag1, int frag1_len,
    const uint8_t *frag2, int frag2_len);

/**
 * @brief Encrypt one AES-128 block.
 *
 * @param key 16-byte AES key.
 * @param dest 16-byte destination block.
 * @param src 16-byte source block.
 * @return true on success, false on failure.
 */
bool meshcore_platform_crypto_aes128_encrypt_block(const uint8_t *key,
                                                   uint8_t *dest,
                                                   const uint8_t *src);

/**
 * @brief Decrypt one AES-128 block.
 *
 * @param key 16-byte AES key.
 * @param dest 16-byte destination block.
 * @param src 16-byte source block.
 * @return true on success, false on failure.
 */
bool meshcore_platform_crypto_aes128_decrypt_block(const uint8_t *key,
                                                   uint8_t *dest,
                                                   const uint8_t *src);

/**
 * @brief Compute HMAC-SHA256.
 *
 * @param mac Destination MAC buffer.
 * @param mac_len Size of @p mac in bytes.
 * @param key Key bytes.
 * @param key_len Number of bytes in @p key.
 * @param msg Message bytes, or NULL when @p msg_len is 0.
 * @param msg_len Number of bytes in @p msg.
 * @return true on success, false on failure.
 */
bool meshcore_platform_crypto_hmac_sha256(uint8_t *mac, size_t mac_len,
                                          const uint8_t *key, size_t key_len,
                                          const uint8_t *msg, size_t msg_len);

/** @} */

/**
 * @name Host-Owned Node State
 * @{
 */

/**
 * @brief Get the local node identity.
 *
 * @param[out] out Local identity to fill.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_node_identity_get(meshcore_common_node_identity_t *out);

/**
 * @brief Get the last local node configuration modification time.
 *
 * @param[out] out_timestamp Modification timestamp in seconds.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_node_config_last_modify_get(uint32_t *out_timestamp);

/**
 * @brief Get local runtime routing policy.
 *
 * @param[out] out Runtime policy to fill.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_node_runtime_policy_get(
    meshcore_common_node_runtime_policy_t *out);

/**
 * @brief Get local advert profile data.
 *
 * @param[out] out Advert profile to fill.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_node_advert_profile_get(
    meshcore_common_node_advert_profile_t *out);

/** @} */

/**
 * @name Host-Owned Peer And Channel State
 * @{
 */

/**
 * @brief Look up an outbound path for a peer public key.
 *
 * @param public_key Peer public key.
 * @param[out] out Peer path to fill.
 * @return 0 on success, -ENOENT when no path is known, or another negative
 * errno-style value.
 */
int meshcore_platform_peer_path_get_by_key(
    const uint8_t *public_key, meshcore_common_peer_path_t *out);

/**
 * @brief Record that a peer was seen.
 *
 * @param public_key Peer public key.
 * @param has_snr true when @p snr_q4 is valid.
 * @param snr_q4 Last seen SNR in quarter-dB units.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_peer_seen_update(const uint8_t *public_key, bool has_snr,
                                       int8_t snr_q4);

/**
 * @brief Iterate peer shared secrets by hash.
 *
 * @param hash Secret hash to match.
 * @param start_slot Host-defined slot to start searching from.
 * @param[out] slot_id Matched slot identifier for the next search.
 * @param[out] dest_secret Matched channel/peer secret bytes.
 * @param[out] peer_identity Peer identity associated with the secret.
 * @return 0 on success, -ENOENT when no later match exists, or another negative
 * errno-style value.
 */
int meshcore_platform_peer_next_shared_secret_by_hash(
    const uint8_t *hash, size_t start_slot, size_t *slot_id,
    uint8_t *dest_secret, meshcore_common_peer_identity_t *peer_identity);

/**
 * @brief Test whether a channel secret matches a channel hash.
 *
 * @param channel_hash One-byte channel hash.
 * @param secret Channel secret bytes.
 * @param secret_len Number of bytes in @p secret.
 * @return Nonzero if the secret matches, 0 if it does not, or a negative
 * errno-style value.
 */
int meshcore_platform_channel_secret_match_exists(uint8_t channel_hash,
                                                  const uint8_t *secret,
                                                  size_t secret_len);

/**
 * @brief Compute a channel hash from a channel secret.
 *
 * @param secret Channel secret bytes.
 * @param secret_len Number of bytes in @p secret.
 * @param[out] out_hash Destination one-byte hash.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_channel_secret_hash(const uint8_t *secret,
                                          size_t secret_len,
                                          uint8_t *out_hash);

/**
 * @brief Search channel records by channel hash.
 *
 * @param hash Channel hash bytes.
 * @param[out] channels Destination channel views, or NULL to query support.
 * @param max_matches Capacity of @p channels in entries.
 * @return Number of matches, 0 when none exist, or a negative errno-style
 * value.
 */
int meshcore_platform_channel_search_by_hash(
    const uint8_t *hash, meshcore_common_channel_view_t *channels,
    int max_matches);

/** @} */

/**
 * @name Optional Dispatcher Diagnostics And Policy Hooks
 * @{
 */

/**
 * @brief Log a raw received frame.
 *
 * @param snr Receive SNR in dB.
 * @param rssi Receive RSSI in dBm.
 * @param raw Serialized frame bytes.
 * @param len Number of bytes in @p raw.
 */
void meshcore_platform_dispatcher_log_rx_raw(float snr, float rssi,
                                             const uint8_t raw[], int len);

/**
 * @brief Log a decoded received packet.
 *
 * @param packet Borrowed packet view.
 * @param len Serialized packet length.
 * @param score Host-computed packet score.
 */
void meshcore_platform_dispatcher_log_rx(
    const meshcore_common_packet_view_t *packet, int len, float score);

/**
 * @brief Log a decoded transmitted packet.
 *
 * @param packet Borrowed packet view.
 * @param len Serialized packet length.
 */
void meshcore_platform_dispatcher_log_tx(
    const meshcore_common_packet_view_t *packet, int len);

/**
 * @brief Log a decoded packet that failed to transmit.
 *
 * @param packet Borrowed packet view.
 * @param len Serialized packet length.
 */
void meshcore_platform_dispatcher_log_tx_fail(
    const meshcore_common_packet_view_t *packet, int len);

/**
 * @brief Get dispatcher airtime budget factor.
 *
 * @return Host-defined budget factor.
 */
float meshcore_platform_dispatcher_airtime_budget_factor_get(void);

/**
 * @brief Calculate a receive deferral delay.
 *
 * @param score Packet score from meshcore_platform_radio_packet_score().
 * @param air_time Estimated airtime in milliseconds.
 * @return Delay in milliseconds.
 */
int meshcore_platform_dispatcher_rx_delay_calc(float score, uint32_t air_time);

/**
 * @brief Get maximum CAD failure duration.
 *
 * @return Duration in milliseconds.
 */
uint32_t meshcore_platform_dispatcher_cad_fail_max_duration_get(void);

/**
 * @brief Get host interference threshold.
 *
 * @return Host-defined threshold.
 */
int meshcore_platform_dispatcher_interference_threshold_get(void);

/**
 * @brief Get AGC reset interval.
 *
 * @return Interval in milliseconds, or 0 to disable.
 */
int meshcore_platform_dispatcher_agc_reset_interval_get(void);

/**
 * @brief Get dispatcher duty-cycle accounting window.
 *
 * @return Window in milliseconds, or 0 to disable host accounting.
 */
unsigned long meshcore_platform_dispatcher_duty_cycle_window_ms_get(void);

/** @} */

/**
 * @name Optional Mesh Policy Hooks
 * @{
 */

/**
 * @brief Get retry delay after CAD failure.
 *
 * @return Delay in milliseconds.
 */
uint32_t meshcore_platform_mesh_cad_fail_retry_delay_get(void);

/**
 * @brief Decide whether a received flood packet should be filtered.
 *
 * @param packet Borrowed packet view, or NULL for default-query calls.
 * @return true to drop the packet, false to continue processing.
 */
bool meshcore_platform_mesh_filter_recv_flood_packet(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Decide whether a packet may be forwarded.
 *
 * @param packet Borrowed packet view, or NULL for default-query calls.
 * @return true to allow forwarding.
 */
bool meshcore_platform_mesh_allow_packet_forward(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Get retransmit delay for a flood packet.
 *
 * @param packet Borrowed packet view, or NULL for default-query calls.
 * @return Delay in milliseconds.
 */
uint32_t meshcore_platform_mesh_retransmit_delay_get(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Get retransmit delay for a direct packet.
 *
 * @param packet Borrowed packet view, or NULL for default-query calls.
 * @return Delay in milliseconds.
 */
uint32_t meshcore_platform_mesh_direct_retransmit_delay_get(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Get number of extra ACK transmissions.
 *
 * @return Extra ACK transmit count.
 */
uint8_t meshcore_platform_mesh_extra_ack_transmit_count_get(void);

/** @} */

/**
 * @name Lower-Level Protocol Receive Hooks
 *
 * These hooks are used by the runtime bridge to publish decoded protocol
 * callbacks. Pointer arguments are borrowed for the duration of the call unless
 * explicitly documented otherwise.
 *
 * @{
 */

/**
 * @brief Publish decrypted peer data to the host.
 *
 * @param packet Borrowed packet view.
 * @param type Peer data type.
 * @param sender Borrowed sender identity.
 * @param secret Borrowed matched secret bytes.
 * @param data Borrowed payload bytes.
 * @param len Number of bytes in @p data.
 */
void meshcore_platform_mesh_on_peer_data_recv(
    const meshcore_common_packet_view_t *packet, uint8_t type,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *data, size_t len);

/**
 * @brief Publish a received trace-path response.
 *
 * @param packet Borrowed packet view.
 * @param tag Trace correlation tag.
 * @param auth_code Upstream trace authentication code.
 * @param flags Upstream trace flags.
 * @param path_snrs Borrowed path SNR bytes.
 * @param path_hashes Borrowed path hash bytes.
 * @param path_len Number of path entries.
 */
void meshcore_platform_mesh_on_trace_recv(
    const meshcore_common_packet_view_t *packet, uint32_t tag,
    uint32_t auth_code, uint8_t flags, const uint8_t *path_snrs,
    const uint8_t *path_hashes, uint8_t path_len);

/**
 * @brief Publish and optionally accept a peer path update.
 *
 * @param packet Borrowed packet view.
 * @param sender Borrowed sender identity.
 * @param secret Borrowed matched secret bytes.
 * @param path Borrowed path bytes.
 * @param path_len Number of bytes in @p path.
 * @param extra_type Extra field type.
 * @param extra Borrowed extra field bytes.
 * @param extra_len Number of bytes in @p extra.
 * @return true if the runtime should accept the path update.
 */
bool meshcore_platform_mesh_on_peer_path_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len);

/**
 * @brief Publish a received advert.
 *
 * @param packet Borrowed packet view.
 * @param identity Borrowed advertised identity.
 * @param timestamp Advert timestamp.
 * @param app_data Borrowed app-data bytes.
 * @param app_data_len Number of bytes in @p app_data.
 */
void meshcore_platform_mesh_on_advert_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_identity_view_t *identity, uint32_t timestamp,
    const uint8_t *app_data, size_t app_data_len);

/**
 * @brief Publish decrypted anonymous data.
 *
 * @param packet Borrowed packet view.
 * @param secret Borrowed matched secret bytes.
 * @param sender Borrowed sender identity.
 * @param data Borrowed payload bytes.
 * @param len Number of bytes in @p data.
 */
void meshcore_platform_mesh_on_anon_data_recv(
    const meshcore_common_packet_view_t *packet, const uint8_t *secret,
    const meshcore_common_identity_view_t *sender, uint8_t *data, size_t len);

/**
 * @brief Publish a received path response.
 *
 * @param packet Borrowed packet view.
 * @param sender Borrowed sender identity.
 * @param path Borrowed path bytes.
 * @param path_len Number of bytes in @p path.
 * @param extra_type Extra field type.
 * @param extra Borrowed extra field bytes.
 * @param extra_len Number of bytes in @p extra.
 */
void meshcore_platform_mesh_on_path_recv(
    const meshcore_common_packet_view_t *packet,
    const meshcore_common_identity_view_t *sender, uint8_t *path,
    uint8_t path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len);

/**
 * @brief Publish a decoded control packet.
 *
 * @param packet Borrowed packet view.
 */
void meshcore_platform_mesh_on_control_data_recv(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Publish a decoded raw custom packet.
 *
 * @param packet Borrowed packet view.
 */
void meshcore_platform_mesh_on_raw_data_recv(
    const meshcore_common_packet_view_t *packet);

/**
 * @brief Publish decrypted group/channel data.
 *
 * @param packet Borrowed packet view.
 * @param type Group data type.
 * @param channel Borrowed channel view.
 * @param data Borrowed payload bytes.
 * @param len Number of bytes in @p data.
 */
void meshcore_platform_mesh_on_group_data_recv(
    const meshcore_common_packet_view_t *packet, uint8_t type,
    const meshcore_common_channel_view_t *channel, uint8_t *data, size_t len);

/**
 * @brief Publish a received ACK.
 *
 * @param packet Borrowed packet view.
 * @param ack_crc ACK CRC value.
 */
void meshcore_platform_mesh_on_ack_recv(
    const meshcore_common_packet_view_t *packet, uint32_t ack_crc);

/** @} */

/**
 * @name Runtime Event Publication Hooks
 * @{
 */

/**
 * @brief Publish a runtime request error.
 *
 * @param request_type MeshCore request type.
 * @param err_code Negative errno-style error code.
 */
void meshcore_platform_runtime_request_error(uint8_t request_type,
                                             int err_code);

/**
 * @brief Publish a received text message.
 *
 * @param message Message event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_message(const meshcore_common_message_t *message);

/**
 * @brief Publish a message ACK event.
 *
 * @param target Peer key prefix.
 * @param attempt Caller-provided ACK attempt/correlation token.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_message_ack(const uint8_t *target,
                                        uint8_t attempt);

/**
 * @brief Publish a received advert.
 *
 * @param advert Advert event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_advert(const meshcore_common_advert_event_t *advert);

/**
 * @brief Publish a path-discovery result.
 *
 * @param peer_path Peer path event.
 * @param is_discover true when produced by an explicit discovery request.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_peer_path_publish(
    const meshcore_common_peer_path_event_t *peer_path, bool is_discover);

/**
 * @brief Publish a peer path event.
 *
 * @param peer_path Peer path event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_peer_path(
    const meshcore_common_peer_path_event_t *peer_path);

/**
 * @brief Publish a trace-path event.
 *
 * @param key_prefix Peer public-key prefix.
 * @param state Host-visible trace state.
 * @param tag Trace correlation tag.
 * @param out_path_snr Outbound path SNR values.
 * @param out_count Number of entries in @p out_path_snr.
 * @param return_path_snr Return path SNR values.
 * @param return_count Number of entries in @p return_path_snr.
 * @param has_response_snr true when @p response_snr is valid.
 * @param response_snr Response SNR in quarter-dB units.
 * @param timestamp Event timestamp.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_trace_path(const uint8_t *key_prefix,
                                       uint8_t state,
                                       uint32_t tag,
                                       const int8_t *out_path_snr,
                                       uint8_t out_count,
                                       const int8_t *return_path_snr,
                                       uint8_t return_count,
                                       bool has_response_snr,
                                       int8_t response_snr,
                                       uint32_t timestamp);

/**
 * @brief Publish a telemetry response.
 *
 * @param key_prefix Peer public-key prefix.
 * @param timestamp Event timestamp.
 * @param tag Correlation tag.
 * @param payload Encoded telemetry payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_telemetry(const uint8_t *key_prefix,
                                      uint32_t timestamp, uint32_t tag,
                                      const uint8_t *payload,
                                      size_t payload_len);

/**
 * @brief Publish a binary service response.
 *
 * @param key_prefix Peer public-key prefix.
 * @param timestamp Event timestamp.
 * @param tag Correlation tag.
 * @param payload Response payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_binary_response(const uint8_t *key_prefix,
                                            uint32_t timestamp, uint32_t tag,
                                            const uint8_t *payload,
                                            size_t payload_len);

/**
 * @brief Publish a node-discover response.
 *
 * @param event Node-discover response event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_node_discover(
    const meshcore_common_node_discover_event_t *event);

/**
 * @brief Publish a channel binary-data event.
 *
 * @param event Channel data event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_channel_data(
    const meshcore_common_channel_data_event_t *event);

/**
 * @brief Publish a raw custom packet event.
 *
 * @param event Raw data event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_raw_data(
    const meshcore_common_raw_data_event_t *event);

/**
 * @brief Publish a zero-hop control packet event.
 *
 * @param event Control data event.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_platform_event_control_data(
    const meshcore_common_control_data_event_t *event);

/** @} */

/**
 * @name Optional Host Telemetry Source Hook
 * @{
 */

/**
 * @brief Get local telemetry for a remote requester.
 *
 * @param requester Requester identity derived from the packet.
 * @param permission_mask Requested telemetry permission bits.
 * @param[out] out Telemetry payload to fill.
 * @return 0 on success, -ENOSYS when unsupported, or another negative
 * errno-style value.
 */
int meshcore_platform_telemetry_node_get(
    const meshcore_platform_request_source_t *requester,
    uint8_t permission_mask, meshcore_platform_telemetry_payload_t *out);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_PLATFORM_H_ */
