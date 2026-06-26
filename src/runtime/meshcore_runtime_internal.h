// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_RUNTIME_INTERNAL_H_
#define MESHCORE_RUNTIME_INTERNAL_H_

#include "meshcore/runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore_advert_data.h"
#include "meshcore_clock.h"
#include "meshcore_group_channel.h"
#include "meshcore_identity.h"
#include "meshcore_mesh.h"
#include "meshcore_packet.h"
#include "meshcore_packet_manager.h"
#include "meshcore_radio.h"
#include "meshcore_tables.h"
#include "meshcore_utils.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef MESHCORE_RUNTIME_PACKET_POOL_SIZE
#define MESHCORE_RUNTIME_PACKET_POOL_SIZE 10U
#endif
#ifndef MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS
#define MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS 10000UL
#endif
#ifndef MESHCORE_RUNTIME_TXT_ACK_DELAY_MS
#define MESHCORE_RUNTIME_TXT_ACK_DELAY_MS 200U
#endif
#ifndef MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS
#define MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS 300U
#endif

#define MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE 8U
#define MESHCORE_RUNTIME_PATH_HASH_SIZE_DEFAULT 1U

#define MESHCORE_RUNTIME_TXT_TYPE_PLAIN 0U
#define MESHCORE_RUNTIME_TXT_TYPE_CLI_DATA 1U
#define MESHCORE_RUNTIME_TXT_TYPE_SIGNED_PLAIN 2U
#define MESHCORE_RUNTIME_TELEM_PERM_SUPPORTED                                  \
  (MESHCORE_TELEM_PERM_BASE | MESHCORE_TELEM_PERM_LOCATION |                  \
   MESHCORE_TELEM_PERM_ENVIRONMENT)
#define MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA 0x03U
#define MESHCORE_RUNTIME_REQ_TYPE_DISCOVER_PATH_SNR 0x07U
#define MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_REQ 0x80U
#define MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_RESP 0x90U

enum meshcore_runtime_request_type {
  MESHCORE_RUNTIME_REQUEST_NONE = 0,
  MESHCORE_RUNTIME_REQUEST_NODE_ADVERT,
  MESHCORE_RUNTIME_REQUEST_NODE_PEER_ADVERT,
  MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE,
  MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL,
  MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH,
  MESHCORE_RUNTIME_REQUEST_NODE_TRACE_PATH,
  MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY,
  MESHCORE_RUNTIME_REQUEST_NODE_BINARY,
  MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER,
  MESHCORE_RUNTIME_REQUEST_CHANNEL_DATA,
  MESHCORE_RUNTIME_REQUEST_RAW_DATA,
  MESHCORE_RUNTIME_REQUEST_CONTROL_DATA,
  MESHCORE_RUNTIME_REQUEST_NODE_BINARY_RESPONSE,
  MESHCORE_RUNTIME_REQUEST_NODE_ANON_DATA,
};

struct meshcore_runtime_request_node_advert {
  bool flood;
};

struct meshcore_runtime_request_node_peer_advert {
  uint8_t raw_advert[MESHCORE_MAX_RAW_ADVERT_LEN];
  size_t raw_advert_len;
};

struct meshcore_runtime_request_message_send_to_node {
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  bool flood;
  uint8_t attempt;
  uint8_t payload[MESHCORE_MAX_MESSAGE_TX_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_message_send_to_channel {
  uint8_t secret[MESHCORE_CHANNEL_SECRET_MAX_LEN];
  size_t secret_len;
  uint8_t payload[MESHCORE_MAX_MESSAGE_TX_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_node_discover_path {
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint32_t tag;
};

struct meshcore_runtime_request_node_trace_path {
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  uint8_t path_len;
  uint8_t path_hash_size;
  uint32_t tag;
};

struct meshcore_runtime_request_node_telemetry {
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t permission_mask;
  uint32_t tag;
};

struct meshcore_runtime_request_node_binary {
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t payload[MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN];
  size_t payload_len;
  uint32_t tag;
};

struct meshcore_runtime_request_node_anon_data {
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t payload[MESHCORE_MAX_ANON_DATA_PAYLOAD_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_node_binary_response {
  meshcore_common_binary_request_event_t request;
  uint8_t payload[MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_node_discover {
  uint8_t filter;
  bool prefix_only;
  uint32_t since;
  uint32_t tag;
};

struct meshcore_runtime_request_channel_data {
  uint8_t secret[MESHCORE_CHANNEL_SECRET_MAX_LEN];
  size_t secret_len;
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  uint8_t path_len;
  uint16_t data_type;
  uint8_t payload[MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_raw_data {
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  uint8_t path_len;
  uint8_t payload[MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN];
  size_t payload_len;
};

struct meshcore_runtime_request_control_data {
  uint8_t payload[MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN];
  size_t payload_len;
};

union meshcore_runtime_request_data {
  struct meshcore_runtime_request_node_advert node_advert;
  struct meshcore_runtime_request_node_peer_advert node_peer_advert;
  struct meshcore_runtime_request_message_send_to_node message_send_to_node;
  struct meshcore_runtime_request_message_send_to_channel
      message_send_to_channel;
  struct meshcore_runtime_request_node_discover_path node_discover_path;
  struct meshcore_runtime_request_node_trace_path node_trace_path;
  struct meshcore_runtime_request_node_telemetry node_telemetry;
  struct meshcore_runtime_request_node_binary node_binary;
  struct meshcore_runtime_request_node_discover node_discover;
  struct meshcore_runtime_request_channel_data channel_data;
  struct meshcore_runtime_request_raw_data raw_data;
  struct meshcore_runtime_request_control_data control_data;
  struct meshcore_runtime_request_node_binary_response node_binary_response;
  struct meshcore_runtime_request_node_anon_data node_anon_data;
};

struct meshcore_runtime_request_slot {
  bool used;
  uint8_t type;
  union meshcore_runtime_request_data data;
};

struct meshcore_runtime_expected_ack {
  bool valid;
  unsigned long msg_sent_ms;
  unsigned long expires_at_ms;
  uint32_t ack_crc;
  uint8_t target[MESHCORE_NODE_KEY_PREFIX_BYTES];
  uint8_t attempt;
};

struct meshcore_runtime_pending_discovery {
  bool valid;
  uint32_t tag;
  unsigned long expires_at_ms;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
};

struct meshcore_runtime_pending_trace {
  bool valid;
  uint32_t tag;
  unsigned long expires_at_ms;
};

struct meshcore_runtime_pending_telemetry {
  bool valid;
  uint32_t tag;
  unsigned long expires_at_ms;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
  uint8_t permission_mask;
};

struct meshcore_runtime_pending_binary {
  bool valid;
  uint32_t tag;
  unsigned long expires_at_ms;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
};

struct meshcore_runtime {
  bool initialized;
  uint32_t last_now_ms;

  struct meshcore_packet_queue_manager packet_manager;
  struct meshcore_tables tables;
  struct meshcore_mesh mesh;
  struct meshcore_rtc_clock_state rtc_clock_state;

  struct meshcore_runtime_expected_ack
      expected_acks[MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE];
  size_t expected_ack_next;
  struct meshcore_runtime_pending_discovery pending_discovery;
  struct meshcore_runtime_pending_trace pending_trace;
  struct meshcore_runtime_pending_telemetry pending_telemetry;
  struct meshcore_runtime_pending_binary pending_binary;
};

struct meshcore_runtime *meshcore_runtime_context_get(void);

int meshcore_runtime_require_initialized(void);
void meshcore_runtime_timer_sync(uint32_t now_ms);
bool meshcore_runtime_time_reached(unsigned long now_ms,
                                   unsigned long expires_at_ms);
bool meshcore_runtime_deadline_accumulate(uint32_t now_ms,
                                          uint32_t candidate_ms,
                                          bool *has_deadline,
                                          uint32_t *deadline_ms);
uint32_t meshcore_runtime_timestamp_now_seconds(void);
bool meshcore_runtime_path_len_decode(uint8_t path_len_field,
                                      uint8_t *out_path_bytes,
                                      uint8_t *out_path_hash_size);
bool meshcore_runtime_extract_return_path_snrs(
    const struct meshcore_packet *packet, int8_t *snrs, uint8_t *hop_count);
void meshcore_runtime_append_snr_with_trunc(int8_t *snrs, uint8_t *len,
                                            uint8_t max_len, int8_t snr_q4);
uint8_t meshcore_runtime_role_to_advert_type(
    meshcore_common_node_role_t role);
uint8_t meshcore_runtime_normalize_path_hash_size(uint8_t path_hash_size);
uint8_t meshcore_runtime_local_path_hash_size_get(void);
bool meshcore_runtime_path_len_encode(uint8_t path_hash_size,
                                      uint8_t path_byte_len,
                                      uint8_t *out_path_len);
int meshcore_runtime_sync_local_identity(void);
bool meshcore_runtime_local_identity_get(
    meshcore_common_node_identity_t *out);
bool meshcore_runtime_local_role_is(meshcore_common_node_role_t role);
bool meshcore_runtime_peer_path_get(const uint8_t *public_key,
                                    meshcore_common_peer_path_t *out,
                                    uint8_t *out_path_len);

bool meshcore_runtime_protocol_allow_packet_forward(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet);
uint32_t meshcore_runtime_protocol_get_retransmit_delay(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet);
uint32_t meshcore_runtime_protocol_get_direct_retransmit_delay(
    void *user_data, struct meshcore_mesh *mesh,
    const struct meshcore_packet *packet);

void meshcore_runtime_text_message_publish(
    const meshcore_common_peer_identity_t *peer,
    const struct meshcore_packet *packet, uint32_t sender_timestamp,
    const uint8_t *text, size_t text_len);
uint8_t meshcore_runtime_packet_path_copy(
    const struct meshcore_packet *packet, uint8_t *dest, size_t dest_len);
void meshcore_runtime_channel_text_publish(
    const struct meshcore_group_channel *channel,
    const struct meshcore_packet *packet, uint32_t sender_timestamp,
    const uint8_t *text, size_t text_len);
void meshcore_runtime_channel_data_publish(
    const struct meshcore_group_channel *channel,
    const struct meshcore_packet *packet, const uint8_t *data, size_t len);
void meshcore_runtime_control_data_publish(struct meshcore_packet *packet);
void meshcore_runtime_node_discover_publish(struct meshcore_packet *packet);
void meshcore_runtime_raw_data_publish(struct meshcore_packet *packet);
void meshcore_runtime_binary_request_publish(
    const meshcore_common_peer_identity_t *peer,
    const struct meshcore_packet *packet, uint32_t tag, const uint8_t *payload,
    size_t payload_len);
void meshcore_runtime_peer_path_publish(
    struct meshcore_packet *packet, const meshcore_common_peer_identity_t *peer,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len);
bool meshcore_runtime_advert_publish(
    const struct meshcore_packet *packet,
    const struct meshcore_identity *identity, uint32_t timestamp,
    const struct meshcore_advert_data_parser *parser);

bool meshcore_runtime_handle_control_discover_request(
    struct meshcore_packet *packet);

void meshcore_runtime_correlation_cleanup(unsigned long now_ms);
bool meshcore_runtime_correlation_next_deadline_get(uint32_t now_ms,
                                                    uint32_t *deadline_ms);
void meshcore_runtime_expected_ack_register(uint32_t ack_crc,
                                            const uint8_t *target,
                                            uint8_t attempt);
bool meshcore_runtime_expected_ack_handle(uint32_t ack_crc);
void meshcore_runtime_pending_discovery_clear(void);
void meshcore_runtime_pending_trace_clear(void);
void meshcore_runtime_pending_telemetry_clear(void);
void meshcore_runtime_pending_binary_clear(void);
void meshcore_runtime_pending_discovery_register(uint32_t tag,
                                                 const uint8_t *key_prefix);
void meshcore_runtime_pending_trace_register(uint32_t tag);
void meshcore_runtime_pending_telemetry_register(uint32_t tag,
                                                 const uint8_t *key_prefix,
                                                 uint8_t permission_mask);
void meshcore_runtime_pending_binary_register(uint32_t tag,
                                              const uint8_t *key_prefix);
bool meshcore_runtime_pending_discovery_handle(
    const uint8_t *key_prefix, struct meshcore_packet *packet, uint8_t *path,
    uint8_t path_len_field, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len);
bool meshcore_runtime_pending_trace_handle(uint32_t tag, uint8_t flags,
                                           const uint8_t *path_snrs,
                                           uint8_t path_snr_count,
                                           uint8_t path_hash_bytes,
                                           int8_t response_snr);
bool meshcore_runtime_pending_telemetry_handle(const uint8_t *key_prefix,
                                               uint32_t timestamp,
                                               const uint8_t *payload,
                                               size_t payload_len);
bool meshcore_runtime_pending_binary_handle(const uint8_t *key_prefix,
                                            uint32_t timestamp,
                                            const uint8_t *payload,
                                            size_t payload_len);

#endif /* MESHCORE_RUNTIME_INTERNAL_H_ */
