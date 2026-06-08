// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_MESH_H_
#define MESHCORE_CORE_MESH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore_dispatcher.h"
#include "meshcore_group_channel.h"
#include "meshcore_identity.h"
#include "meshcore_packet_manager.h"
#include "meshcore_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal mesh behavior layer for meshcore-C.
 *
 * This header maps upstream Mesh.h methods into C entry points while host
 * integration is routed through the runtime platform bridge.
 */

struct meshcore_mesh;
struct meshcore_common_peer_identity;

struct meshcore_mesh_runtime_ops {
  bool (*allow_packet_forward)(void *user_data, struct meshcore_mesh *mesh,
                               const struct meshcore_packet *packet);
  uint32_t (*get_retransmit_delay)(void *user_data,
                                   struct meshcore_mesh *mesh,
                                   const struct meshcore_packet *packet);
  uint32_t (*get_direct_retransmit_delay)(
      void *user_data, struct meshcore_mesh *mesh,
      const struct meshcore_packet *packet);
  void (*on_peer_data_recv)(void *user_data, struct meshcore_mesh *mesh,
                            struct meshcore_packet *packet, uint8_t type,
                            const struct meshcore_common_peer_identity *sender,
                            const uint8_t *secret, uint8_t *data, size_t len);
  void (*on_trace_recv)(void *user_data, struct meshcore_mesh *mesh,
                        struct meshcore_packet *packet, uint32_t tag,
                        uint32_t auth_code, uint8_t flags,
                        const uint8_t *path_snrs,
                        const uint8_t *path_hashes, uint8_t path_len);
  bool (*on_peer_path_recv)(
      void *user_data, struct meshcore_mesh *mesh,
      struct meshcore_packet *packet,
      const struct meshcore_common_peer_identity *sender,
      const uint8_t *secret, uint8_t *path, uint8_t path_len,
      uint8_t extra_type, uint8_t *extra, uint8_t extra_len);
  void (*on_advert_recv)(void *user_data, struct meshcore_mesh *mesh,
                         struct meshcore_packet *packet,
                         const struct meshcore_identity *identity,
                         uint32_t timestamp, const uint8_t *app_data,
                         size_t app_data_len);
  void (*on_control_data_recv)(void *user_data, struct meshcore_mesh *mesh,
                               struct meshcore_packet *packet);
  void (*on_raw_data_recv)(void *user_data, struct meshcore_mesh *mesh,
                           struct meshcore_packet *packet);
  void (*on_group_data_recv)(void *user_data, struct meshcore_mesh *mesh,
                             struct meshcore_packet *packet, uint8_t type,
                             const struct meshcore_group_channel *channel,
                             uint8_t *data, size_t len);
  void (*on_ack_recv)(void *user_data, struct meshcore_mesh *mesh,
                      struct meshcore_packet *packet, uint32_t ack_crc);
};

struct meshcore_mesh {
  struct meshcore_dispatcher dispatcher;
  struct meshcore_local_identity self_id;
  struct meshcore_packet_queue_manager *packet_manager;
  struct meshcore_tables *tables;
  const struct meshcore_mesh_runtime_ops *runtime_ops;
  void *runtime_user_data;
};

void meshcore_mesh_init(
    struct meshcore_mesh *mesh,
    struct meshcore_packet_queue_manager *packet_manager,
    struct meshcore_tables *tables);
void meshcore_mesh_set_runtime_ops(
    struct meshcore_mesh *mesh, const struct meshcore_mesh_runtime_ops *ops,
    void *user_data);

/* public methods */
void meshcore_mesh_begin(struct meshcore_mesh *mesh);
void meshcore_mesh_loop(struct meshcore_mesh *mesh);
struct meshcore_tables *meshcore_mesh_get_tables(
    const struct meshcore_mesh *mesh);

struct meshcore_packet *meshcore_mesh_create_advert(
    struct meshcore_mesh *mesh,
    const struct meshcore_local_identity *identity, const uint8_t *app_data,
    size_t app_data_len);
struct meshcore_packet *meshcore_mesh_create_datagram(
    struct meshcore_mesh *mesh, uint8_t type,
    const struct meshcore_identity *dest, const uint8_t *secret,
    const uint8_t *data, size_t len);
struct meshcore_packet *meshcore_mesh_create_anon_datagram(
    struct meshcore_mesh *mesh, uint8_t type,
    const struct meshcore_local_identity *sender,
    const struct meshcore_identity *dest, const uint8_t *secret,
    const uint8_t *data, size_t data_len);
struct meshcore_packet *meshcore_mesh_create_group_datagram(
    struct meshcore_mesh *mesh, uint8_t type,
    const struct meshcore_group_channel *channel, const uint8_t *data,
    size_t data_len);
struct meshcore_packet *meshcore_mesh_create_ack(struct meshcore_mesh *mesh,
                                                 uint32_t ack_crc);
struct meshcore_packet *meshcore_mesh_create_multi_ack(
    struct meshcore_mesh *mesh, uint32_t ack_crc, uint8_t remaining);
struct meshcore_packet *meshcore_mesh_create_path_return_by_dest_hash(
    struct meshcore_mesh *mesh, const uint8_t *dest_hash,
    const uint8_t *secret, const uint8_t *path, uint8_t path_len,
    uint8_t extra_type, const uint8_t *extra, size_t extra_len);
struct meshcore_packet *meshcore_mesh_create_path_return_by_identity(
    struct meshcore_mesh *mesh, const struct meshcore_identity *dest,
    const uint8_t *secret, const uint8_t *path, uint8_t path_len,
    uint8_t extra_type, const uint8_t *extra, size_t extra_len);
struct meshcore_packet *meshcore_mesh_create_raw_data(
    struct meshcore_mesh *mesh, const uint8_t *data, size_t len);
struct meshcore_packet *meshcore_mesh_create_trace(struct meshcore_mesh *mesh,
                                                   uint32_t tag,
                                                   uint32_t auth_code,
                                                   uint8_t flags);
struct meshcore_packet *meshcore_mesh_create_control_data(
    struct meshcore_mesh *mesh, const uint8_t *data, size_t len);

/*
 * send helpers consume packet ownership once called with a non-NULL mesh/packet.
 *
 * After passing packet into any send helper below, callers must not free or
 * reuse it again. On success the packet is queued into the dispatcher; on
 * invalid send arguments the mesh layer releases the packet immediately.
 */
void meshcore_mesh_send_flood(struct meshcore_mesh *mesh,
                              struct meshcore_packet *packet,
                              uint32_t delay_millis, uint8_t path_hash_size);
void meshcore_mesh_send_flood_by_transport_codes(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint16_t transport_codes[2], uint32_t delay_millis,
    uint8_t path_hash_size);
void meshcore_mesh_send_direct(struct meshcore_mesh *mesh,
                               struct meshcore_packet *packet,
                               const uint8_t *path, uint8_t path_len,
                               uint32_t delay_millis);
void meshcore_mesh_send_zero_hop(struct meshcore_mesh *mesh,
                                 struct meshcore_packet *packet,
                                 uint32_t delay_millis);
void meshcore_mesh_send_zero_hop_by_transport_codes(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint16_t transport_codes[2], uint32_t delay_millis);

/* protected methods */
meshcore_dispatcher_action meshcore_mesh_on_recv_packet(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet);
uint32_t meshcore_mesh_runtime_get_cad_fail_retry_delay(struct meshcore_mesh *mesh);
meshcore_dispatcher_action meshcore_mesh_route_recv_packet(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet);
bool meshcore_mesh_runtime_filter_recv_flood_packet(struct meshcore_mesh *mesh,
                                                    struct meshcore_packet *packet);
bool meshcore_mesh_runtime_allow_packet_forward(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet);
uint32_t meshcore_mesh_runtime_get_retransmit_delay(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet);
uint32_t meshcore_mesh_runtime_get_direct_retransmit_delay(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet);
uint8_t meshcore_mesh_runtime_get_extra_ack_transmit_count(struct meshcore_mesh *mesh);
int meshcore_mesh_runtime_next_peer_shared_secret_by_hash(
    struct meshcore_mesh *mesh, const uint8_t *hash, size_t start_slot,
    size_t *slot_id, uint8_t *dest_secret,
    struct meshcore_common_peer_identity *peer_identity);
void meshcore_mesh_runtime_on_peer_data_recv(struct meshcore_mesh *mesh,
                                             struct meshcore_packet *packet,
                                             uint8_t type,
                                             const struct meshcore_common_peer_identity *sender,
                                             const uint8_t *secret, uint8_t *data,
                                             size_t len);
void meshcore_mesh_runtime_on_trace_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet, uint32_t tag,
    uint32_t auth_code, uint8_t flags, const uint8_t *path_snrs,
    const uint8_t *path_hashes, uint8_t path_len);
bool meshcore_mesh_runtime_on_peer_path_recv(struct meshcore_mesh *mesh,
                                             struct meshcore_packet *packet,
                                             const struct meshcore_common_peer_identity *sender,
                                             const uint8_t *secret,
                                             uint8_t *path, uint8_t path_len,
                                             uint8_t extra_type, uint8_t *extra,
                                             uint8_t extra_len);
void meshcore_mesh_runtime_on_advert_recv(struct meshcore_mesh *mesh,
                                          struct meshcore_packet *packet,
                                          const struct meshcore_identity *identity,
                                          uint32_t timestamp, const uint8_t *app_data,
                                          size_t app_data_len);
void meshcore_mesh_runtime_on_anon_data_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint8_t *secret, const struct meshcore_identity *sender,
    uint8_t *data, size_t len);
void meshcore_mesh_runtime_on_path_recv(struct meshcore_mesh *mesh,
                                        struct meshcore_packet *packet,
                                        struct meshcore_identity *sender,
                                        uint8_t *path, uint8_t path_len,
                                        uint8_t extra_type, uint8_t *extra,
                                        uint8_t extra_len);
void meshcore_mesh_runtime_on_control_data_recv(struct meshcore_mesh *mesh,
                                                struct meshcore_packet *packet);
void meshcore_mesh_runtime_on_raw_data_recv(struct meshcore_mesh *mesh,
                                            struct meshcore_packet *packet);
int meshcore_mesh_runtime_search_channels_by_hash(
    struct meshcore_mesh *mesh, const uint8_t *hash,
    struct meshcore_group_channel channels[], int max_matches);
void meshcore_mesh_runtime_on_group_data_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data,
    size_t len);
void meshcore_mesh_runtime_on_ack_recv(struct meshcore_mesh *mesh,
                                       struct meshcore_packet *packet,
                                       uint32_t ack_crc);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_MESH_H_ */
