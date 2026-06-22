/*
 * Copyright (c) 2026 FoBE Studio
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MeshCore public constants and shared data types.
 */

#ifndef MESHCORE_TYPES_H_
#define MESHCORE_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup meshcore MeshCore C Library
 * @brief Platform-neutral MeshCore protocol/runtime C API.
 * @{
 * @}
 */

/**
 * @defgroup meshcore_types MeshCore Shared Types
 * @brief Constants, limits, and data views shared by runtime and platform APIs.
 * @ingroup meshcore
 * @{
 */

/** Public ABI version for breaking interface revisions. */
#define MESHCORE_ABI_VERSION 25U

/** Size of an Ed25519 public key in bytes. */
#define MESHCORE_PUBLIC_KEY_SIZE 32U
/** Maximum supported channel secret length in bytes. */
#define MESHCORE_CHANNEL_SECRET_MAX_LEN 32U
/** Maximum text message payload length accepted by the public API. */
#define MESHCORE_MAX_MESSAGE_TX_LEN 160U
/** Maximum service request payload length accepted by the public API. */
#define MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN 168U
/** Maximum service response payload length surfaced to platform events. */
#define MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN 180U
/** Maximum anonymous datagram payload length accepted by the public API. */
#define MESHCORE_MAX_ANON_DATA_PAYLOAD_LEN 136U
/** Maximum group/channel binary payload length. */
#define MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN 165U
/** Maximum raw custom packet payload length. */
#define MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN 184U
/** Maximum zero-hop control packet payload length. */
#define MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN 184U
/** Maximum serialized advert payload length. */
#define MESHCORE_MAX_RAW_ADVERT_LEN 255U
/** Maximum MeshCore transport unit length. */
#define MESHCORE_MAX_TRANS_UNIT_LEN 255U
/** Unknown outbound path sentinel used by companion contacts. */
#define MESHCORE_OUT_PATH_UNKNOWN 0xFFU

/** Reserved group/channel binary data type. */
#define MESHCORE_CHANNEL_DATA_TYPE_RESERVED 0x0000U
/** Device-originated group/channel binary data type. */
#define MESHCORE_CHANNEL_DATA_TYPE_DEV 0xFFFFU

/** Base telemetry permission bit. */
#define MESHCORE_TELEM_PERM_BASE 0x01U
/** Location telemetry permission bit. */
#define MESHCORE_TELEM_PERM_LOCATION 0x02U
/** Environment telemetry permission bit. */
#define MESHCORE_TELEM_PERM_ENVIRONMENT 0x04U

/** Maximum node name length, including any terminating NUL stored by hosts. */
#define MESHCORE_NODE_NAME_MAX_LEN 32U
/** Maximum message sender display-name length. */
#define MESHCORE_MESSAGE_SENDER_NAME_MAX_LEN 32U
#if !defined(MESHCORE_NODE_KEY_PREFIX_BYTES)
/** Number of public-key prefix bytes used by host peer identifiers. */
#define MESHCORE_NODE_KEY_PREFIX_BYTES 4U
#endif
/** Size of an Ed25519 private key in bytes. */
#define MESHCORE_PRIVATE_KEY_SIZE 64U
/** Number of channel-hash bytes carried by public channel views. */
#define MESHCORE_CHANNEL_HASH_BYTES 1U
/** Number of peer key-prefix bytes accepted by message events. */
#define MESHCORE_MESSAGE_TARGET_PREFIX_BYTES 4U
/** Supported 16-byte channel secret length. */
#define MESHCORE_CHANNEL_SECRET_LEN_16 16U
/** Supported 32-byte channel secret length. */
#define MESHCORE_CHANNEL_SECRET_LEN_32 32U
/** Maximum encoded path byte count. */
#define MESHCORE_MAX_PATH_LEN 64U
/** Maximum decoded telemetry payload bytes. */
#define MESHCORE_MAX_TELEMETRY_PAYLOAD_LEN 230U
/** Maximum channel matches returned by channel hash search. */
#define MESHCORE_CHANNEL_SEARCH_MAX_MATCHES 4U
/** Public-key prefix length used by node-discover prefix responses. */
#define MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES 8U

/** Peer request payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_REQ 0x00U
/** Peer response payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_RESPONSE 0x01U
/** Peer text-message payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_TXT_MSG 0x02U
/** ACK payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_ACK 0x03U
/** Advert payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_ADVERT 0x04U
/** Group text-message payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_GRP_TXT 0x05U
/** Group binary-data payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_GRP_DATA 0x06U
/** Anonymous request payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_ANON_REQ 0x07U
/** Path discovery payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_PATH 0x08U
/** Trace path payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_TRACE 0x09U
/** Multipart payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_MULTIPART 0x0AU
/** Zero-hop control payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_CONTROL 0x0BU
/** Application raw-custom payload type. */
#define MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM 0x0FU

/** Path extra-field type used for SNR records. */
#define MESHCORE_PACKET_PATH_EXTRA_TYPE_SNR 0x0EU

/**
 * @brief Upstream MeshCore node roles.
 */
typedef enum meshcore_common_node_role {
  /** No role advertised. */
  MESHCORE_COMMON_NODE_ROLE_NONE = 0,
  /** Chat/user node. */
  MESHCORE_COMMON_NODE_ROLE_CHAT = 1,
  /** Repeater node. */
  MESHCORE_COMMON_NODE_ROLE_REPEATER = 2,
  /** Room node. */
  MESHCORE_COMMON_NODE_ROLE_ROOM = 3,
  /** Sensor node. */
  MESHCORE_COMMON_NODE_ROLE_SENSOR = 4,
} meshcore_common_node_role_t;

/** Node-discover filter bit for chat/user nodes. */
#define MESHCORE_NODE_DISCOVER_FILTER_CHAT (1U << MESHCORE_COMMON_NODE_ROLE_CHAT)
/** Node-discover filter bit for repeater nodes. */
#define MESHCORE_NODE_DISCOVER_FILTER_REPEATER                                            \
  (1U << MESHCORE_COMMON_NODE_ROLE_REPEATER)
/** Node-discover filter bit for room nodes. */
#define MESHCORE_NODE_DISCOVER_FILTER_ROOM (1U << MESHCORE_COMMON_NODE_ROLE_ROOM)
/** Node-discover filter bit for sensor nodes. */
#define MESHCORE_NODE_DISCOVER_FILTER_SENSOR (1U << MESHCORE_COMMON_NODE_ROLE_SENSOR)
/** Node-discover filter for every upstream role bit. */
#define MESHCORE_NODE_DISCOVER_FILTER_ALL 0xFFU

/**
 * @brief Loop detection policy requested by the host.
 */
typedef enum meshcore_common_loop_detect_mode {
  /** Disable loop detection. */
  MESHCORE_COMMON_LOOP_DETECT_OFF = 0,
  /** Minimal loop detection. */
  MESHCORE_COMMON_LOOP_DETECT_MINIMAL = 1,
  /** Moderate loop detection. */
  MESHCORE_COMMON_LOOP_DETECT_MODERATE = 2,
  /** Strict loop detection. */
  MESHCORE_COMMON_LOOP_DETECT_STRICT = 3,
} meshcore_common_loop_detect_mode_t;

/**
 * @brief Runtime message event type.
 */
typedef enum meshcore_common_message_type {
  /** Message from a peer node. */
  MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_NODE = 1,
  /** Message from a channel/group. */
  MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_CHANNEL = 2,
} meshcore_common_message_type_t;

/**
 * @brief Packet route classification visible to hosts.
 */
typedef enum meshcore_common_message_route {
  /** Route is not known or not applicable. */
  MESHCORE_COMMON_MESSAGE_ROUTE_UNSPECIFIED = 0,
  /** Packet was sent through flood routing. */
  MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD = 1,
  /** Packet was sent over a direct encoded path. */
  MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT = 2,
} meshcore_common_message_route_t;

/**
 * @brief Host-owned local node identity.
 */
typedef struct meshcore_common_node_identity {
  /** NUL-terminated node name when available. */
  char name[MESHCORE_NODE_NAME_MAX_LEN];
  /** Advertised node role. */
  meshcore_common_node_role_t role;
  /** Local public key. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  /** Local private key. */
  uint8_t private_key[MESHCORE_PRIVATE_KEY_SIZE];
} meshcore_common_node_identity_t;

/**
 * @brief Host-owned runtime policy used by mesh routing and scheduling.
 */
typedef struct meshcore_common_node_runtime_policy {
  /** Path hash width in bytes. */
  uint8_t path_hash_size;
  /** Loop detection policy. */
  meshcore_common_loop_detect_mode_t loop_detect;
  /** Enable client packet repeat behavior. */
  bool client_repeat;
  /** Disable packet forwarding. */
  bool disable_fwd;
  /** Maximum flood hop count. */
  uint8_t flood_max;
  /** Number of duplicate ACKs to send when applicable. */
  uint8_t multi_acks;
  /** Flood transmit delay factor. */
  float tx_delay_factor;
  /** Direct transmit delay factor. */
  float direct_tx_delay_factor;
} meshcore_common_node_runtime_policy_t;

/**
 * @brief Host-owned local advert profile.
 */
typedef struct meshcore_common_node_advert_profile {
  /** True when latitude and longitude are valid. */
  bool has_position;
  /** Latitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t latitude;
  /** Longitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t longitude;
  /** Local advert interval in seconds. */
  uint32_t advert_interval;
  /** Flood advert interval in seconds. */
  uint32_t flood_advert_interval;
} meshcore_common_node_advert_profile_t;

/**
 * @brief Public identity view for a peer known by the host.
 */
typedef struct meshcore_common_peer_identity {
  /** NUL-terminated peer name when available. */
  char name[MESHCORE_NODE_NAME_MAX_LEN];
  /** Advertised peer role. */
  meshcore_common_node_role_t role;
  /** Host-owned peer flags. */
  uint8_t flags;
  /** Peer public key. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
} meshcore_common_peer_identity_t;

/**
 * @brief Host-owned outbound path for a peer.
 */
typedef struct meshcore_common_peer_path {
  /** Path byte length, or MESHCORE_OUT_PATH_UNKNOWN when unknown. */
  uint8_t out_path_len;
  /** Encoded outbound path bytes. */
  uint8_t out_path[MESHCORE_MAX_PATH_LEN];
  /** Path hash width in bytes. */
  uint8_t path_hash_size;
} meshcore_common_peer_path_t;

/**
 * @brief Host-owned channel secret.
 */
typedef struct meshcore_common_channel_secret {
  /** Number of valid bytes in @ref secret. */
  uint8_t secret_len;
  /** Channel secret bytes. */
  uint8_t secret[MESHCORE_CHANNEL_SECRET_MAX_LEN];
} meshcore_common_channel_secret_t;

/**
 * @brief Borrowed public-key identity view.
 */
typedef struct meshcore_common_identity_view {
  /** Public key bytes. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
} meshcore_common_identity_view_t;

/**
 * @brief Borrowed channel view.
 */
typedef struct meshcore_common_channel_view {
  /** Channel hash bytes. */
  uint8_t hash[MESHCORE_CHANNEL_HASH_BYTES];
  /** Channel secret bytes. */
  uint8_t secret[MESHCORE_CHANNEL_SECRET_MAX_LEN];
} meshcore_common_channel_view_t;

/**
 * @brief Borrowed read-only packet view for synchronous platform hooks.
 *
 * Pointer fields reference library-owned storage and are only valid until the
 * hook that receives this view returns. Hosts must copy data they need after
 * the callback. @ref raw may be NULL when serialized packet bytes are not
 * available for the current hook.
 */
typedef struct meshcore_common_packet_view {
  /** Packet route classification. */
  meshcore_common_message_route_t route;
  /** MeshCore payload type. */
  uint8_t payload_type;
  /** MeshCore payload version. */
  uint8_t payload_ver;
  /** Logical path hop count. */
  uint8_t path_len;
  /** Path hash width in bytes. */
  uint8_t path_hash_size;
  /** Number of path hashes. */
  uint8_t path_hash_count;
  /** Number of valid bytes in @ref path. */
  uint8_t path_byte_len;
  /** Borrowed encoded path bytes. */
  const uint8_t *path;
  /** Number of valid bytes in @ref payload. */
  uint16_t payload_len;
  /** Borrowed payload bytes. */
  const uint8_t *payload;
  /** Number of valid bytes in @ref raw. */
  uint16_t raw_len;
  /** Borrowed serialized packet bytes, or NULL. */
  const uint8_t *raw;
  /** True when @ref rx_snr_q4 is valid. */
  bool has_rx_snr;
  /** Receive SNR in quarter-dB units. */
  int8_t rx_snr_q4;
  /** True when transport SNR metadata was present. */
  bool has_transport_snr;
  /** True when transport coding metadata was present. */
  bool has_transport_codes;
} meshcore_common_packet_view_t;

/**
 * @brief Text-message event published by the runtime.
 */
typedef struct meshcore_common_message {
  /** Message source type. */
  meshcore_common_message_type_t type;
  /** Message route classification. */
  meshcore_common_message_route_t route;
  /** Number of valid bytes in @ref target. */
  uint8_t target_len;
  /** Peer key prefix for node messages, channel secret prefix for channels. */
  uint8_t target[MESHCORE_MESSAGE_TARGET_PREFIX_BYTES];
  /** NUL-terminated sender name when available. */
  char sender_name[MESHCORE_MESSAGE_SENDER_NAME_MAX_LEN];
  /** Number of valid bytes in @ref payload. */
  uint16_t payload_len;
  /** Message payload bytes. */
  uint8_t payload[MESHCORE_MAX_MESSAGE_TX_LEN];
  /** Sender timestamp when encoded by upstream payload helpers. */
  uint64_t sender_timestamp;
  /** True when @ref rx_snr is valid. */
  bool has_rx_snr;
  /** Receive SNR in dB. */
  float rx_snr;
} meshcore_common_message_t;

/**
 * @brief Channel binary-data event published by the runtime.
 */
typedef struct meshcore_common_channel_data_event {
  /** Channel hash bytes. */
  uint8_t channel_hash[MESHCORE_CHANNEL_HASH_BYTES];
  /** Number of valid bytes in @ref path. */
  uint8_t path_len;
  /** Encoded source path. */
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  /** Application data type. */
  uint16_t data_type;
  /** Number of valid bytes in @ref payload. */
  uint8_t payload_len;
  /** Binary payload bytes. */
  uint8_t payload[MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN];
  /** True when @ref rx_snr_q4 is valid. */
  bool has_rx_snr;
  /** Receive SNR in quarter-dB units. */
  int8_t rx_snr_q4;
} meshcore_common_channel_data_event_t;

/**
 * @brief Raw custom packet event published by the runtime.
 */
typedef struct meshcore_common_raw_data_event {
  /** Number of valid bytes in @ref path. */
  uint8_t path_len;
  /** Encoded source path. */
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  /** Number of valid bytes in @ref payload. */
  uint8_t payload_len;
  /** Raw custom payload bytes. */
  uint8_t payload[MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN];
  /** True when @ref rx_snr_q4 is valid. */
  bool has_rx_snr;
  /** Receive SNR in quarter-dB units. */
  int8_t rx_snr_q4;
} meshcore_common_raw_data_event_t;

/**
 * @brief Zero-hop control packet event published by the runtime.
 */
typedef struct meshcore_common_control_data_event {
  /** Number of valid bytes in @ref path. */
  uint8_t path_len;
  /** Encoded source path. */
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  /** Number of valid bytes in @ref payload. */
  uint8_t payload_len;
  /** Control payload bytes. */
  uint8_t payload[MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN];
  /** True when @ref rx_snr_q4 is valid. */
  bool has_rx_snr;
  /** Receive SNR in quarter-dB units. */
  int8_t rx_snr_q4;
} meshcore_common_control_data_event_t;

/**
 * @brief Binary service request event published by the runtime.
 */
typedef struct meshcore_common_binary_request_event {
  /** Packet route classification for the incoming request. */
  meshcore_common_message_route_t route;
  /** Requester public-key prefix. */
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
  /** Request correlation tag. */
  uint32_t tag;
  /** Requester full public key. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  /** Encoded source path length field from the request packet. */
  uint8_t path_len;
  /** Encoded source path bytes from the request packet. */
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  /** Number of valid bytes in @ref payload. */
  uint8_t payload_len;
  /** Binary request payload bytes, excluding the MeshCore correlation tag. */
  uint8_t payload[MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN];
  /** True when @ref rx_snr_q4 is valid. */
  bool has_rx_snr;
  /** Receive SNR in quarter-dB units. */
  int8_t rx_snr_q4;
} meshcore_common_binary_request_event_t;

/**
 * @brief Node-discover response event published by the runtime.
 */
typedef struct meshcore_common_node_discover_event {
  /** Response node role decoded from the control-data response type. */
  meshcore_common_node_role_t role;
  /** Request correlation tag echoed by the response. */
  uint32_t tag;
  /** Number of valid bytes in @ref public_key. Usually 8 or 32. */
  uint8_t public_key_len;
  /** Response public-key prefix or full public key. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  /** Number of valid bytes in @ref path. */
  uint8_t path_len;
  /** Encoded source path. */
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  /** SNR measured by the responder when it received the request. */
  int8_t uplink_snr;
  /** SNR measured locally when this node received the response. */
  int8_t downlink_snr;
} meshcore_common_node_discover_event_t;

/**
 * @brief Advert event published by the runtime.
 */
typedef struct meshcore_common_advert_event {
  /** NUL-terminated node name when available. */
  char name[MESHCORE_NODE_NAME_MAX_LEN];
  /** Advertised node role. */
  meshcore_common_node_role_t role;
  /** Advertised public key. */
  uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE];
  /** True when the host considered this advert a new peer record. */
  bool is_new;
  /** Advert timestamp. */
  uint32_t advert_timestamp;
  /** True when latitude and longitude are valid. */
  bool has_position;
  /** Latitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t latitude;
  /** Longitude encoded with upstream MeshCore fixed-point semantics. */
  int32_t longitude;
  /** True when outbound path fields are valid. */
  bool has_out_path;
  /** Number of valid bytes in @ref out_path. */
  uint8_t out_path_len;
  /** Path hash width in bytes. */
  uint8_t path_hash_size;
  /** Encoded outbound path. */
  uint8_t out_path[MESHCORE_MAX_PATH_LEN];
  /** Number of valid bytes in @ref raw_advert. */
  uint8_t raw_advert_len;
  /** Serialized upstream advert bytes. */
  uint8_t raw_advert[MESHCORE_MAX_RAW_ADVERT_LEN];
} meshcore_common_advert_event_t;

/**
 * @brief Peer path event published by the runtime.
 */
typedef struct meshcore_common_peer_path_event {
  /** Request correlation tag. */
  uint32_t tag;
  /** Peer public-key prefix. */
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES];
  /** Path timestamp. */
  uint32_t timestamp;
  /** Last seen SNR in quarter-dB units. */
  int8_t last_seen_snr;
  /** Number of valid bytes in @ref out_path. */
  uint8_t out_path_len;
  /** Path hash width in bytes. */
  uint8_t path_hash_size;
  /** Encoded outbound path. */
  uint8_t out_path[MESHCORE_MAX_PATH_LEN];
  /** Number of valid entries in @ref out_path_snr. */
  uint8_t out_path_snr_count;
  /** Outbound path SNR values in quarter-dB units. */
  int8_t out_path_snr[MESHCORE_MAX_PATH_LEN];
  /** Number of valid entries in @ref return_path_snr. */
  uint8_t return_path_snr_count;
  /** Return path SNR values in quarter-dB units. */
  int8_t return_path_snr[MESHCORE_MAX_PATH_LEN];
} meshcore_common_peer_path_event_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_TYPES_H_ */
