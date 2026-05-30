// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_PACKET_H_
#define MESHCORE_CORE_PACKET_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal packet model for meshcore-C.
 *
 * This header intentionally mirrors the source/c_impl packet vocabulary so
 * future upgrades can be tracked module-by-module against meshcore_ref.
 */

#define PH_ROUTE_MASK 0x03U
#define PH_TYPE_SHIFT 2U
#define PH_TYPE_MASK 0x0FU
#define PH_VER_SHIFT 6U
#define PH_VER_MASK 0x03U

#define ROUTE_TYPE_TRANSPORT_FLOOD 0x00U
#define ROUTE_TYPE_FLOOD 0x01U
#define ROUTE_TYPE_DIRECT 0x02U
#define ROUTE_TYPE_TRANSPORT_DIRECT 0x03U

#define PAYLOAD_TYPE_REQ 0x00U
#define PAYLOAD_TYPE_RESPONSE 0x01U
#define PAYLOAD_TYPE_TXT_MSG 0x02U
#define PAYLOAD_TYPE_ACK 0x03U
#define PAYLOAD_TYPE_ADVERT 0x04U
#define PAYLOAD_TYPE_GRP_TXT 0x05U
#define PAYLOAD_TYPE_GRP_DATA 0x06U
#define PAYLOAD_TYPE_ANON_REQ 0x07U
#define PAYLOAD_TYPE_PATH 0x08U
#define PAYLOAD_TYPE_TRACE 0x09U
#define PAYLOAD_TYPE_MULTIPART 0x0AU
#define PAYLOAD_TYPE_CONTROL 0x0BU
#define PAYLOAD_TYPE_RAW_CUSTOM 0x0FU

#define PAYLOAD_VER_1 0x00U
#define PAYLOAD_VER_2 0x01U
#define PAYLOAD_VER_3 0x02U
#define PAYLOAD_VER_4 0x03U

#define PATH_EXTRA_TYPE_SNR 0x0EU

#define MESHCORE_PACKET_HASH_SIZE 8U
#define MESHCORE_PACKET_PAYLOAD_MAX_LEN 184U

#define MESHCORE_SNR_TRANSPORT_CODE0 0x534EU
#define MESHCORE_SNR_TRANSPORT_CODE1 0x0001U

struct meshcore_packet {
  uint8_t header;
  uint16_t payload_len;
  uint16_t path_len;
  uint16_t transport_codes[2];
  uint8_t path[MESHCORE_MAX_PATH_LEN];
  uint8_t payload[MESHCORE_PACKET_PAYLOAD_MAX_LEN];
  int8_t snr_q4;
};

void meshcore_packet_init(struct meshcore_packet *packet);

uint8_t meshcore_packet_get_route_type(const struct meshcore_packet *packet);
bool meshcore_packet_is_route_flood(const struct meshcore_packet *packet);
bool meshcore_packet_is_route_direct(const struct meshcore_packet *packet);
bool meshcore_packet_has_transport_codes(const struct meshcore_packet *packet);
bool meshcore_packet_has_snr_flag(const struct meshcore_packet *packet);

uint8_t meshcore_packet_get_payload_type(const struct meshcore_packet *packet);
uint8_t meshcore_packet_get_payload_ver(const struct meshcore_packet *packet);

uint8_t meshcore_packet_get_path_hash_size(const struct meshcore_packet *packet);
uint8_t meshcore_packet_get_path_hash_count(const struct meshcore_packet *packet);
uint8_t meshcore_packet_get_path_byte_len(const struct meshcore_packet *packet);
void meshcore_packet_set_path_hash_count(struct meshcore_packet *packet,
                                         uint8_t hash_count);
void meshcore_packet_set_path_hash_size_and_count(
    struct meshcore_packet *packet, uint8_t hash_size, uint8_t hash_count);

bool meshcore_packet_is_valid_path_len(uint8_t path_len);
size_t meshcore_packet_write_path(uint8_t *dest, const uint8_t *src,
                                  uint8_t path_len);
uint8_t meshcore_packet_copy_path(uint8_t *dest, const uint8_t *src,
                                  uint8_t path_len);

void meshcore_packet_mark_do_not_retransmit(struct meshcore_packet *packet);
bool meshcore_packet_is_marked_do_not_retransmit(
    const struct meshcore_packet *packet);

float meshcore_packet_get_snr(const struct meshcore_packet *packet);

int meshcore_packet_get_raw_length(const struct meshcore_packet *packet);

/*
 * Internal duplicate-filter / ACK-match helper.
 * This is not a public cryptographic API.
 */
void meshcore_packet_calculate_hash(const struct meshcore_packet *packet,
                                    uint8_t *hash);

uint8_t meshcore_packet_write_to(const struct meshcore_packet *packet,
                                 uint8_t dest[]);
bool meshcore_packet_read_from(struct meshcore_packet *packet,
                               const uint8_t src[], uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_PACKET_H_ */
