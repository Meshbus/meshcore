/*
 * Copyright (c) 2026 FoBE Studio
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Host-callable MeshCore runtime API.
 */

#ifndef MESHCORE_RUNTIME_H_
#define MESHCORE_RUNTIME_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup meshcore_runtime MeshCore Runtime API
 * @brief Host-callable singleton MeshCore runtime functions.
 * @ingroup meshcore
 *
 * This header is the callable protocol-engine boundary. It exposes no product
 * SDK, RTOS, host-storage, transport, or C++ dependency surface.
 *
 * Runtime model:
 * - single process-wide runtime instance per node
 * - single-threaded and non-reentrant
 * - host-driven through typed requests, radio RX/TX injection, and timer expiry
 *
 * Callback rules:
 * - platform callbacks run synchronously from meshcore_*() entry points
 * - callbacks must not call back into meshcore_*() APIs
 * - hosts that need blocking work should enqueue it outside this library
 *
 * Host implementation hooks are declared by @ref meshcore_platform. The runtime
 * binds directly to those link-time hooks.
 *
 * @{
 */

/**
 * @brief Initialize the process-wide MeshCore runtime.
 *
 * The host must link concrete hook implementations declared by
 * meshcore/platform.h before this function can be used.
 *
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_init(void);

/**
 * @brief Release runtime resources.
 *
 * After deinitialization, the host must call meshcore_init() again before using
 * other runtime entry points.
 */
void meshcore_deinit(void);

/**
 * @brief Inject a fired runtime timer.
 *
 * @param now_ms Current host uptime in milliseconds.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_timer_fired(uint32_t now_ms);

/**
 * @brief Inject a received raw radio frame.
 *
 * @param data Serialized frame bytes.
 * @param len Number of bytes in @p data.
 * @param rssi_dbm Receive RSSI in dBm.
 * @param snr_q4 Receive SNR in quarter-dB units.
 * @param now_ms Current host uptime in milliseconds.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_radio_rx_inject(const uint8_t *data, size_t len,
                             int16_t rssi_dbm, int8_t snr_q4,
                             uint32_t now_ms);

/**
 * @brief Inject radio TX completion for the active outbound frame.
 *
 * @param now_ms Current host uptime in milliseconds.
 * @param success true when radio transmission completed successfully.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_radio_tx_done(uint32_t now_ms, bool success);

/**
 * @brief Request a local advert transmission.
 *
 * @param flood true to publish as a flood advert, false for local routing.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_advert_request(bool flood);

/**
 * @brief Request replay of a raw advert captured by the host.
 *
 * @param raw_advert Serialized advert bytes.
 * @param raw_advert_len Number of bytes in @p raw_advert.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_peer_advert_request(const uint8_t *raw_advert,
                                      size_t raw_advert_len);

/**
 * @brief Send a text message to a peer public key.
 *
 * @param public_key Full peer public key.
 * @param flood true to force flood routing; false to prefer a known path.
 * @param attempt Caller-provided ACK correlation token.
 * @param payload Message payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_message_send_to_node(const uint8_t *public_key, bool flood,
                                  uint8_t attempt,
                                  const uint8_t *payload,
                                  size_t payload_len);

/**
 * @brief Send a text message to a channel.
 *
 * @param secret Full channel secret.
 * @param secret_len Number of bytes in @p secret.
 * @param payload Message payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_message_send_to_channel(const uint8_t *secret, size_t secret_len,
                                     const uint8_t *payload,
                                     size_t payload_len);

/**
 * @brief Send a binary datagram to a channel.
 *
 * @param secret Full channel secret.
 * @param secret_len Number of bytes in @p secret.
 * @param path Optional encoded path bytes, or NULL when @p path_len is 0.
 * @param path_len Number of bytes in @p path.
 * @param data_type Application data type.
 * @param payload Datagram payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_channel_data_send(const uint8_t *secret, size_t secret_len,
                               const uint8_t *path, uint8_t path_len,
                               uint16_t data_type, const uint8_t *payload,
                               size_t payload_len);

/**
 * @brief Request path discovery for a peer.
 *
 * @param public_key Full peer public key.
 * @param request_tag Optional request correlation tag storage. Pass NULL or
 * a pointer to zero to let the runtime generate a tag. A non-zero pointed
 * value is used as the request tag. On success, the actual request tag is
 * written back to this pointer when provided.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_discover_path_request(const uint8_t *public_key,
                                        uint32_t *request_tag);

/**
 * @brief Request trace path for a peer with an existing outbound path.
 *
 * @param public_key Full peer public key.
 * @param request_tag Optional request correlation tag storage. Pass NULL or
 * a pointer to zero to let the runtime generate a tag. A non-zero pointed
 * value is used as the request tag. On success, the actual request tag is
 * written back to this pointer when provided.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_trace_path_request(const uint8_t *public_key,
                                     uint32_t *request_tag);

/**
 * @brief Request telemetry from a peer.
 *
 * @param public_key Full peer public key.
 * @param permission_mask MeshCore telemetry permission bits. Wire encoding
 * uses the upstream inverse-mask request byte, @c ~permission_mask.
 * @param request_tag Optional request correlation tag storage. Pass NULL or
 * a pointer to zero to let the runtime generate a tag. A non-zero pointed
 * value is used as the request tag. On success, the actual request tag is
 * written back to this pointer when provided.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_telemetry_request(const uint8_t *public_key,
                                    uint8_t permission_mask,
                                    uint32_t *request_tag);

/**
 * @brief Request remote binary data.
 *
 * The runtime generates a correlation tag.
 *
 * @param public_key Full peer public key.
 * @param payload Request payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_binary_request(const uint8_t *public_key,
                                 const uint8_t *payload,
                                 size_t payload_len);

/**
 * @brief Request remote binary data with a caller-provided correlation tag.
 *
 * @param public_key Full peer public key.
 * @param payload Request payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @param tag Caller-provided correlation tag.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_node_binary_request_with_tag(const uint8_t *public_key,
                                          const uint8_t *payload,
                                          size_t payload_len,
                                          uint32_t tag);

/**
 * @brief Send an application raw-custom packet over an encoded path.
 *
 * @param path Encoded direct path bytes.
 * @param path_len Number of bytes in @p path.
 * @param payload Raw-custom payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_raw_data_send(const uint8_t *path, uint8_t path_len,
                           const uint8_t *payload, size_t payload_len);

/**
 * @brief Send a zero-hop control packet.
 *
 * Payload byte 0 must have bit 7 set.
 *
 * @param payload Control payload bytes.
 * @param payload_len Number of bytes in @p payload.
 * @return 0 on success, or a negative errno-style value.
 */
int meshcore_control_data_send(const uint8_t *payload, size_t payload_len);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_RUNTIME_H_ */
