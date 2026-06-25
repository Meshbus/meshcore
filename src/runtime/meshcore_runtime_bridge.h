// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_RUNTIME_BRIDGE_H_
#define MESHCORE_RUNTIME_BRIDGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct meshcore_packet;
struct meshcore_identity;
struct meshcore_common_peer_identity;
struct meshcore_group_channel;

#ifdef __cplusplus
extern "C" {
#endif

void meshcore_runtime_on_ack_recv(struct meshcore_packet *packet,
                                  uint32_t ack_crc);
void meshcore_runtime_on_peer_data_recv(struct meshcore_packet *packet,
                                        uint8_t type,
                                        const struct meshcore_common_peer_identity *sender,
                                        const uint8_t *secret, uint8_t *data,
                                        size_t len);
bool meshcore_runtime_on_peer_path_recv(struct meshcore_packet *packet,
                                        const struct meshcore_common_peer_identity *sender,
                                        const uint8_t *secret, uint8_t *path,
                                        uint8_t path_len, uint8_t extra_type,
                                        uint8_t *extra, uint8_t extra_len);
void meshcore_runtime_on_advert_recv(struct meshcore_packet *packet,
                                     const struct meshcore_identity *identity,
                                     uint32_t timestamp,
                                     const uint8_t *app_data,
                                     size_t app_data_len);
void meshcore_runtime_on_control_data_recv(struct meshcore_packet *packet);
void meshcore_runtime_on_raw_data_recv(struct meshcore_packet *packet);
void meshcore_runtime_on_group_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data, size_t len);
void meshcore_runtime_on_trace_recv(struct meshcore_packet *packet,
                                    uint32_t tag, uint32_t auth_code,
                                    uint8_t flags, const uint8_t *path_snrs,
                                    const uint8_t *path_hashes,
                                    uint8_t path_len);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_RUNTIME_BRIDGE_H_ */
