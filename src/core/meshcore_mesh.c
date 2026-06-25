// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_mesh.h"

#include <string.h>

#include "meshcore_clock.h"
#include "meshcore_rng.h"
#include "meshcore_platform_bridge.h"
#include "meshcore_utils.h"

#define MESHCORE_MESH_CIPHER_MAC_SIZE 2U
#define MESHCORE_MESH_CIPHER_BLOCK_SIZE 16U
#define MESHCORE_MESH_PATH_HASH_SIZE 1U
#define MESHCORE_MESH_SIGNATURE_SIZE 64U
#define MESHCORE_MESH_MAX_ADVERT_DATA_SIZE 32U
#define MESHCORE_MESH_MAX_COMBINED_PATH \
  (MESHCORE_PACKET_PAYLOAD_MAX_LEN - MESHCORE_MESH_CIPHER_MAC_SIZE - \
   MESHCORE_MESH_CIPHER_BLOCK_SIZE)

static void meshcore_mesh_rng_random(struct meshcore_mesh *mesh, uint8_t *dest,
                                     size_t size)
{
  (void)mesh;

  if (dest == NULL || size == 0U) {
    return;
  }

  meshcore_rng_random(dest, size);
}

static uint32_t meshcore_mesh_rtc_get_current_time(struct meshcore_mesh *mesh)
{
  (void)mesh;
  return meshcore_clock_rtc_get_current_time();
}

static bool meshcore_mesh_tables_has_seen(struct meshcore_mesh *mesh,
                                          const struct meshcore_packet *packet)
{
  if (mesh == NULL || packet == NULL || mesh->tables == NULL) {
    return false;
  }

  return meshcore_tables_has_seen(mesh->tables, packet);
}

static struct meshcore_packet *meshcore_mesh_obtain_new_packet(
    struct meshcore_mesh *mesh)
{
  if (mesh == NULL) {
    return NULL;
  }

  return meshcore_dispatcher_obtain_new_packet(&mesh->dispatcher);
}

static uint8_t meshcore_mesh_transport_flood_hash_size(uint8_t path_hash_size,
                                                       const uint16_t transport_codes[2])
{
  uint8_t hash_size = path_hash_size;

  if (transport_codes == NULL) {
    return hash_size;
  }
  if (transport_codes[0] == MESHCORE_SNR_TRANSPORT_CODE0 &&
      transport_codes[1] == MESHCORE_SNR_TRANSPORT_CODE1 &&
      hash_size < 3U) {
    hash_size = (uint8_t)(hash_size + 1U);
  }

  return hash_size;
}

static void meshcore_mesh_release_consumed_packet(struct meshcore_mesh *mesh,
                                                  struct meshcore_packet *packet)
{
  if (mesh == NULL || packet == NULL) {
    return;
  }

  meshcore_dispatcher_release_packet(&mesh->dispatcher, packet);
}

static void meshcore_mesh_remove_self_from_path(struct meshcore_packet *packet)
{
  uint8_t sz;
  uint8_t hash_count;
  uint8_t k;

  if (packet == NULL) {
    return;
  }

  hash_count = meshcore_packet_get_path_hash_count(packet);
  if (hash_count == 0U) {
    return;
  }

  meshcore_packet_set_path_hash_count(packet, (uint8_t)(hash_count - 1U));
  sz = meshcore_packet_get_path_hash_size(packet);
  hash_count = meshcore_packet_get_path_hash_count(packet);

  for (k = 0U; k < hash_count * sz; k += sz) {
    memmove(&packet->path[k], &packet->path[k + sz], sz);
  }
}

static void meshcore_mesh_route_direct_recv_acks(struct meshcore_mesh *mesh,
                                                 struct meshcore_packet *packet,
                                                 uint32_t delay_millis)
{
  uint8_t extra;
  struct meshcore_packet *a1;
  struct meshcore_packet *a2;

  if (mesh == NULL || packet == NULL) {
    return;
  }

  if (meshcore_packet_is_marked_do_not_retransmit(packet)) {
    return;
  }

  extra = meshcore_mesh_runtime_get_extra_ack_transmit_count(mesh);
  while (extra > 0U) {
    delay_millis += meshcore_mesh_runtime_get_direct_retransmit_delay(mesh, packet) + 300U;
    a1 = meshcore_mesh_create_multi_ack_data(mesh, packet->payload,
                                             packet->payload_len, extra);
    if (a1 != NULL) {
      a1->path_len =
          meshcore_packet_copy_path(a1->path, packet->path, (uint8_t)packet->path_len);
      a1->header &= (uint8_t)~PH_ROUTE_MASK;
      a1->header |= ROUTE_TYPE_DIRECT;
      meshcore_dispatcher_send_packet(&mesh->dispatcher, a1, 0U, delay_millis);
    }
    extra--;
  }

  a2 = meshcore_mesh_create_ack_data(mesh, packet->payload, packet->payload_len);
  if (a2 != NULL) {
    a2->path_len =
        meshcore_packet_copy_path(a2->path, packet->path, (uint8_t)packet->path_len);
    a2->header &= (uint8_t)~PH_ROUTE_MASK;
    a2->header |= ROUTE_TYPE_DIRECT;
    meshcore_dispatcher_send_packet(&mesh->dispatcher, a2, 0U, delay_millis);
  }
}

static meshcore_dispatcher_action meshcore_mesh_forward_multipart_direct(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet)
{
  uint8_t remaining;
  uint8_t type;
  struct meshcore_packet tmp;

  if (mesh == NULL || packet == NULL || packet->payload_len == 0U) {
    return MESHCORE_ACTION_RELEASE;
  }

  remaining = (uint8_t)(packet->payload[0] >> 4);
  type = packet->payload[0] & 0x0FU;
  if (type == PAYLOAD_TYPE_ACK && packet->payload_len >= 5U) {
    meshcore_packet_init(&tmp);
    tmp.header = packet->header;
    tmp.path_len = meshcore_packet_copy_path(tmp.path, packet->path, (uint8_t)packet->path_len);
    tmp.payload_len = (uint16_t)(packet->payload_len - 1U);
    memcpy(tmp.payload, &packet->payload[1], tmp.payload_len);

    if (!meshcore_mesh_tables_has_seen(mesh, &tmp)) {
      meshcore_mesh_remove_self_from_path(&tmp);
      meshcore_mesh_route_direct_recv_acks(mesh, &tmp,
                                           ((uint32_t)remaining + 1U) * 300U);
    }
  }

  return MESHCORE_ACTION_RELEASE;
}

void meshcore_mesh_init(
    struct meshcore_mesh *mesh,
    struct meshcore_packet_queue_manager *packet_manager,
    struct meshcore_tables *tables)
{
  if (mesh == NULL) {
    return;
  }

  memset(mesh, 0, sizeof(*mesh));

  mesh->packet_manager = packet_manager;
  mesh->tables = tables;
  meshcore_dispatcher_init(&mesh->dispatcher, packet_manager, mesh);
}

void meshcore_mesh_set_runtime_ops(
    struct meshcore_mesh *mesh, const struct meshcore_mesh_runtime_ops *ops,
    void *user_data)
{
  if (mesh == NULL) {
    return;
  }

  mesh->runtime_ops = ops;
  mesh->runtime_user_data = user_data;
}

void meshcore_mesh_begin(struct meshcore_mesh *mesh)
{
  if (mesh == NULL) {
    return;
  }

  meshcore_dispatcher_begin(&mesh->dispatcher);
}

void meshcore_mesh_loop(struct meshcore_mesh *mesh)
{
  if (mesh == NULL) {
    return;
  }

  meshcore_dispatcher_loop(&mesh->dispatcher);
}

struct meshcore_tables *meshcore_mesh_get_tables(const struct meshcore_mesh *mesh)
{
  if (mesh == NULL) {
    return NULL;
  }

  return mesh->tables;
}

meshcore_dispatcher_action meshcore_mesh_on_recv_packet(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet)
{
  meshcore_dispatcher_action action = MESHCORE_ACTION_RELEASE;
  uint8_t payload_type;
  uint8_t i;
  uint32_t delay;
  uint32_t ack_crc = 0U;

  if (mesh == NULL || packet == NULL) {
    return MESHCORE_ACTION_RELEASE;
  }

  payload_type = meshcore_packet_get_payload_type(packet);

  if (meshcore_packet_is_route_direct(packet) && payload_type == PAYLOAD_TYPE_TRACE) {
    if (packet->path_len < MESHCORE_MAX_PATH_LEN && packet->payload_len >= 9U) {
      uint32_t trace_tag;
      uint32_t auth_code;
      uint8_t flags;
      uint8_t path_sz;
      uint8_t len;
      uint16_t offset;

      i = 0U;
      memcpy(&trace_tag, &packet->payload[i], sizeof(trace_tag));
      i += 4U;
      memcpy(&auth_code, &packet->payload[i], sizeof(auth_code));
      i += 4U;
      flags = packet->payload[i++];
      path_sz = flags & 0x03U;

      len = (uint8_t)(packet->payload_len - i);
      offset = (uint16_t)packet->path_len << path_sz;
      if (offset >= len) {
        meshcore_mesh_runtime_on_trace_recv(mesh, packet, trace_tag, auth_code, flags,
                                    packet->path, &packet->payload[i], len);
      } else if (meshcore_identity_is_hash_match_by_len(
                     &mesh->self_id.identity, &packet->payload[i + offset],
                     (uint8_t)(1U << path_sz)) &&
                 meshcore_mesh_runtime_allow_packet_forward(mesh, packet) &&
                 !meshcore_mesh_tables_has_seen(mesh, packet)) {
        packet->path[packet->path_len++] = (int8_t)(meshcore_packet_get_snr(packet) * 4.0f);
        delay = meshcore_mesh_runtime_get_direct_retransmit_delay(mesh, packet);
        return MESHCORE_ACTION_RETRANSMIT_DELAYED(5U, delay);
      }
    }
    return MESHCORE_ACTION_RELEASE;
  }

  if (meshcore_packet_is_route_direct(packet) &&
      payload_type == PAYLOAD_TYPE_CONTROL && packet->payload_len > 0U &&
      (packet->payload[0] & 0x80U) != 0U) {
    if (meshcore_packet_get_path_hash_count(packet) == 0U) {
      meshcore_mesh_runtime_on_control_data_recv(mesh, packet);
    }
    return MESHCORE_ACTION_RELEASE;
  }

  if (meshcore_packet_is_route_direct(packet) &&
      meshcore_packet_get_path_hash_count(packet) > 0U) {
    if (payload_type == PAYLOAD_TYPE_ACK && packet->payload_len >= sizeof(ack_crc)) {
      memcpy(&ack_crc, packet->payload, sizeof(ack_crc));
      meshcore_mesh_runtime_on_ack_recv(mesh, packet, ack_crc);
    }

    if (meshcore_identity_is_hash_match_by_len(
            &mesh->self_id.identity, packet->path,
            meshcore_packet_get_path_hash_size(packet)) &&
        meshcore_mesh_runtime_allow_packet_forward(mesh, packet)) {
      if (payload_type == PAYLOAD_TYPE_MULTIPART) {
        return meshcore_mesh_forward_multipart_direct(mesh, packet);
      }
      if (payload_type == PAYLOAD_TYPE_ACK) {
        if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
          meshcore_mesh_remove_self_from_path(packet);
          meshcore_mesh_route_direct_recv_acks(mesh, packet, 0U);
        }
        return MESHCORE_ACTION_RELEASE;
      }

      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        meshcore_mesh_remove_self_from_path(packet);
        delay = meshcore_mesh_runtime_get_direct_retransmit_delay(mesh, packet);
        return MESHCORE_ACTION_RETRANSMIT_DELAYED(0U, delay);
      }
    }
    return MESHCORE_ACTION_RELEASE;
  }

  if (meshcore_packet_is_route_flood(packet) &&
      meshcore_mesh_runtime_filter_recv_flood_packet(mesh, packet)) {
    return MESHCORE_ACTION_RELEASE;
  }

  switch (payload_type) {
    case PAYLOAD_TYPE_ACK:
      if (packet->payload_len < 4U) {
        break;
      }
      memcpy(&ack_crc, packet->payload, sizeof(ack_crc));
      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        meshcore_mesh_runtime_on_ack_recv(mesh, packet, ack_crc);
        action = meshcore_mesh_route_recv_packet(mesh, packet);
      }
      break;
    case PAYLOAD_TYPE_PATH:
    case PAYLOAD_TYPE_REQ:
    case PAYLOAD_TYPE_RESPONSE:
    case PAYLOAD_TYPE_TXT_MSG: {
      uint8_t dest_hash;
      uint8_t src_hash;
      uint8_t *mac_and_data;

      if (packet->payload_len < 2U) {
        break;
      }

      i = 0U;
      dest_hash = packet->payload[i++];
      src_hash = packet->payload[i++];
      mac_and_data = &packet->payload[i];
      if (i + MESHCORE_MESH_CIPHER_MAC_SIZE >= packet->payload_len) {
        break;
      }

      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        if (meshcore_identity_is_hash_match(&mesh->self_id.identity, &dest_hash)) {
          size_t cursor = 0U;
          size_t sender_slot = 0U;
          bool found = false;
          uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
          meshcore_common_peer_identity_t sender = {0};

          while (meshcore_mesh_runtime_next_peer_shared_secret_by_hash(
                     mesh, &src_hash, cursor, &sender_slot, secret, &sender) == 0) {
            uint8_t data[MESHCORE_PACKET_PAYLOAD_MAX_LEN];
            int len;

            cursor = sender_slot + 1U;
            len = meshcore_utils_mac_then_decrypt(
                secret, data, mac_and_data, (int)(packet->payload_len - i));
            if (len <= 0) {
              continue;
            }

            if (payload_type == PAYLOAD_TYPE_PATH) {
              int k = 0;
              uint8_t path_len;
              uint8_t hash_size;
              uint8_t hash_count;
              uint8_t *path;
              uint8_t extra_type;
              uint8_t *extra;
              uint8_t extra_len;

              if (len < 2) {
                continue;
              }

              path_len = data[k++];
              hash_size = (uint8_t)((path_len >> 6) + 1U);
              hash_count = path_len & 63U;
              if (k + hash_size * hash_count + 1 > len) {
                continue;
              }

              path = &data[k];
              k += hash_size * hash_count;
              extra_type = data[k++] & 0x0FU;
              extra = &data[k];
              extra_len = (uint8_t)(len - k);

              if (meshcore_mesh_runtime_on_peer_path_recv(
                      mesh, packet, &sender, secret, path, path_len,
                      extra_type, extra, extra_len) &&
                  meshcore_packet_is_route_flood(packet)) {
                struct meshcore_packet *rpath =
                    meshcore_mesh_create_path_return_by_dest_hash(
                        mesh, &src_hash, secret, packet->path,
                        (uint8_t)packet->path_len, 0U, NULL, 0U);
                if (rpath != NULL) {
                  meshcore_mesh_send_direct(mesh, rpath, path, path_len, 500U);
                }
              }
            } else {
              meshcore_mesh_runtime_on_peer_data_recv(
                  mesh, packet, payload_type, &sender, secret, data,
                  (size_t)len);
            }

            found = true;
            break;
          }

          if (found) {
            meshcore_packet_mark_do_not_retransmit(packet);
          }
        }

        action = meshcore_mesh_route_recv_packet(mesh, packet);
      }
      break;
    }
    case PAYLOAD_TYPE_ANON_REQ: {
      uint8_t dest_hash;
      uint8_t *sender_pub_key;
      uint8_t *mac_and_data;

      if (packet->payload_len < (MESHCORE_MESH_PATH_HASH_SIZE + MESHCORE_PUBLIC_KEY_SIZE)) {
        break;
      }

      i = 0U;
      dest_hash = packet->payload[i++];
      sender_pub_key = &packet->payload[i];
      i += MESHCORE_PUBLIC_KEY_SIZE;
      mac_and_data = &packet->payload[i];
      if (i + MESHCORE_MESH_CIPHER_MAC_SIZE >= packet->payload_len) {
        break;
      }

      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        if (meshcore_identity_is_hash_match(&mesh->self_id.identity, &dest_hash)) {
          struct meshcore_identity sender;
          uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
          uint8_t data[MESHCORE_PACKET_PAYLOAD_MAX_LEN];
          int len;

          meshcore_identity_init_from_pub_key(&sender, sender_pub_key);
          meshcore_local_identity_calc_shared_secret(&mesh->self_id, secret,
                                                     sender.pub_key);
          len = meshcore_utils_mac_then_decrypt(
              secret, data, mac_and_data, (int)(packet->payload_len - i));
          if (len > 0) {
            meshcore_mesh_runtime_on_anon_data_recv(mesh, packet, secret, &sender, data,
                                            (size_t)len);
            meshcore_packet_mark_do_not_retransmit(packet);
          }
        }
        action = meshcore_mesh_route_recv_packet(mesh, packet);
      }
      break;
    }
    case PAYLOAD_TYPE_GRP_DATA:
    case PAYLOAD_TYPE_GRP_TXT: {
      uint8_t channel_hash;
      uint8_t *mac_and_data;

      if (packet->payload_len < MESHCORE_MESH_PATH_HASH_SIZE) {
        break;
      }

      i = 0U;
      channel_hash = packet->payload[i++];
      mac_and_data = &packet->payload[i];
      if (i + MESHCORE_MESH_CIPHER_MAC_SIZE >= packet->payload_len) {
        break;
      }

      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        struct meshcore_group_channel channels[4];
        int num = meshcore_mesh_runtime_search_channels_by_hash(mesh, &channel_hash, channels,
                                                        4);
        int j;

        for (j = 0; j < num; j++) {
          uint8_t data[MESHCORE_PACKET_PAYLOAD_MAX_LEN];
          int len = meshcore_utils_mac_then_decrypt(
              channels[j].secret, data, mac_and_data, (int)(packet->payload_len - i));
          if (len > 0) {
            meshcore_mesh_runtime_on_group_data_recv(mesh, packet, payload_type, &channels[j],
                                             data, (size_t)len);
            break;
          }
        }

        action = meshcore_mesh_route_recv_packet(mesh, packet);
      }
      break;
    }
    case PAYLOAD_TYPE_ADVERT: {
      struct meshcore_identity identity;
      uint32_t timestamp;
      const uint8_t *signature;

      if (packet->payload_len <
          MESHCORE_PUBLIC_KEY_SIZE + 4U + MESHCORE_MESH_SIGNATURE_SIZE) {
        break;
      }

      i = 0U;
      meshcore_identity_init_from_pub_key(&identity, &packet->payload[i]);
      i += MESHCORE_PUBLIC_KEY_SIZE;
      memcpy(&timestamp, &packet->payload[i], sizeof(timestamp));
      i += 4U;
      signature = &packet->payload[i];
      i += MESHCORE_MESH_SIGNATURE_SIZE;

      if (i > packet->payload_len) {
        break;
      }

      if (meshcore_identity_matches_by_pub_key(&mesh->self_id.identity,
                                               identity.pub_key)) {
        break;
      }

      if (!meshcore_mesh_tables_has_seen(mesh, packet)) {
        uint8_t *app_data = &packet->payload[i];
        int app_data_len = (int)(packet->payload_len - i);
        bool is_ok;
        uint8_t message[MESHCORE_PUBLIC_KEY_SIZE + 4U +
                        MESHCORE_MESH_MAX_ADVERT_DATA_SIZE];
        int msg_len = 0;

        if (app_data_len > (int)MESHCORE_MESH_MAX_ADVERT_DATA_SIZE) {
          app_data_len = (int)MESHCORE_MESH_MAX_ADVERT_DATA_SIZE;
        }

        memcpy(&message[msg_len], identity.pub_key, MESHCORE_PUBLIC_KEY_SIZE);
        msg_len += MESHCORE_PUBLIC_KEY_SIZE;
        memcpy(&message[msg_len], &timestamp, 4U);
        msg_len += 4;
        if (app_data_len > 0) {
          memcpy(&message[msg_len], app_data, (size_t)app_data_len);
          msg_len += app_data_len;
        }

        is_ok = meshcore_identity_verify(&identity, signature, message, msg_len);
        if (is_ok) {
          meshcore_mesh_runtime_on_advert_recv(mesh, packet, &identity, timestamp, app_data,
                                       (size_t)app_data_len);
          action = meshcore_mesh_route_recv_packet(mesh, packet);
        }
      }
      break;
    }
    case PAYLOAD_TYPE_RAW_CUSTOM:
      if (meshcore_packet_is_route_direct(packet) &&
          !meshcore_mesh_tables_has_seen(mesh, packet)) {
        meshcore_mesh_runtime_on_raw_data_recv(mesh, packet);
      }
      break;
    case PAYLOAD_TYPE_MULTIPART:
      if (packet->payload_len > 2U) {
        uint8_t remaining = (uint8_t)(packet->payload[0] >> 4);
        uint8_t type = packet->payload[0] & 0x0FU;

        (void)remaining;
        if (type == PAYLOAD_TYPE_ACK && packet->payload_len >= 5U) {
          struct meshcore_packet tmp;

          meshcore_packet_init(&tmp);
          tmp.header = packet->header;
          tmp.path_len = meshcore_packet_copy_path(tmp.path, packet->path,
                                                   (uint8_t)packet->path_len);
          tmp.payload_len = (uint16_t)(packet->payload_len - 1U);
          memcpy(tmp.payload, &packet->payload[1], tmp.payload_len);

          if (!meshcore_mesh_tables_has_seen(mesh, &tmp) &&
              tmp.payload_len >= sizeof(ack_crc)) {
            memcpy(&ack_crc, tmp.payload, sizeof(ack_crc));
            meshcore_mesh_runtime_on_ack_recv(mesh, &tmp, ack_crc);
          }
        }
      }
      break;
    default:
      break;
  }

  return action;
}

uint32_t meshcore_mesh_runtime_get_cad_fail_retry_delay(struct meshcore_mesh *mesh)
{
  (void)mesh;
  return meshcore_platform_bridge_mesh_cad_fail_retry_delay_get();
}

meshcore_dispatcher_action meshcore_mesh_route_recv_packet(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet)
{
  uint8_t hash_count;
  uint8_t hash_size;
  uint32_t delay;

  if (mesh == NULL || packet == NULL) {
    return MESHCORE_ACTION_RELEASE;
  }

  hash_count = meshcore_packet_get_path_hash_count(packet);
  hash_size = meshcore_packet_get_path_hash_size(packet);
  if (meshcore_packet_is_route_flood(packet) &&
      !meshcore_packet_is_marked_do_not_retransmit(packet) &&
      (uint32_t)(hash_count + 1U) * hash_size <= MESHCORE_MAX_PATH_LEN &&
      meshcore_mesh_runtime_allow_packet_forward(mesh, packet)) {
    meshcore_identity_copy_hash_by_len(
        &mesh->self_id.identity, &packet->path[(size_t)hash_count * hash_size],
        hash_size);
    meshcore_packet_set_path_hash_count(packet, (uint8_t)(hash_count + 1U));

    delay = meshcore_mesh_runtime_get_retransmit_delay(mesh, packet);
    return MESHCORE_ACTION_RETRANSMIT_DELAYED(
        meshcore_packet_get_path_hash_count(packet), delay);
  }

  return MESHCORE_ACTION_RELEASE;
}

struct meshcore_packet *meshcore_mesh_create_advert(
    struct meshcore_mesh *mesh, const struct meshcore_local_identity *identity,
    const uint8_t *app_data, size_t app_data_len)
{
  struct meshcore_packet *packet;
  uint32_t emitted_timestamp;
  uint8_t *signature;
  int len = 0;
  uint8_t message[MESHCORE_PUBLIC_KEY_SIZE + 4U + MESHCORE_MESH_MAX_ADVERT_DATA_SIZE];
  int msg_len = 0;

  if (mesh == NULL || identity == NULL || app_data_len > MESHCORE_MESH_MAX_ADVERT_DATA_SIZE ||
      (app_data_len > 0U && app_data == NULL)) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT);
  memcpy(&packet->payload[len], identity->identity.pub_key, MESHCORE_PUBLIC_KEY_SIZE);
  len += MESHCORE_PUBLIC_KEY_SIZE;

  emitted_timestamp = meshcore_mesh_rtc_get_current_time(mesh);
  memcpy(&packet->payload[len], &emitted_timestamp, 4U);
  len += 4;

  signature = &packet->payload[len];
  len += MESHCORE_MESH_SIGNATURE_SIZE;

  if (app_data_len > 0U) {
    memcpy(&packet->payload[len], app_data, app_data_len);
    len += (int)app_data_len;
  }
  packet->payload_len = (uint16_t)len;

  memcpy(&message[msg_len], identity->identity.pub_key, MESHCORE_PUBLIC_KEY_SIZE);
  msg_len += MESHCORE_PUBLIC_KEY_SIZE;
  memcpy(&message[msg_len], &emitted_timestamp, 4U);
  msg_len += 4;
  if (app_data_len > 0U) {
    memcpy(&message[msg_len], app_data, app_data_len);
    msg_len += (int)app_data_len;
  }
  meshcore_local_identity_sign(identity, signature, message, msg_len);

  return packet;
}

struct meshcore_packet *meshcore_mesh_create_datagram(
    struct meshcore_mesh *mesh, uint8_t type, const struct meshcore_identity *dest,
    const uint8_t *secret, const uint8_t *data, size_t len)
{
  struct meshcore_packet *packet;
  static const uint8_t empty_payload = 0U;
  int enc_len;
  int offset = 0;

  if (mesh == NULL || dest == NULL || secret == NULL ||
      (data == NULL && len > 0U)) {
    return NULL;
  }

  if (!(type == PAYLOAD_TYPE_TXT_MSG || type == PAYLOAD_TYPE_REQ ||
        type == PAYLOAD_TYPE_RESPONSE)) {
    return NULL;
  }
  if (!meshcore_mesh_datagram_plaintext_fits(len)) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  if (data == NULL) {
    data = &empty_payload;
  }

  packet->header = (uint8_t)(type << PH_TYPE_SHIFT);
  offset += meshcore_identity_copy_hash_to(dest, &packet->payload[offset]);
  offset += meshcore_identity_copy_hash_to(&mesh->self_id.identity,
                                           &packet->payload[offset]);
  enc_len = meshcore_utils_encrypt_then_mac(secret, &packet->payload[offset], data,
                                            (int)len);
  if (enc_len < 0) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return NULL;
  }
  offset += enc_len;
  packet->payload_len = (uint16_t)offset;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_anon_datagram(
    struct meshcore_mesh *mesh, uint8_t type,
    const struct meshcore_local_identity *sender,
    const struct meshcore_identity *dest, const uint8_t *secret,
    const uint8_t *data, size_t data_len)
{
  struct meshcore_packet *packet;
  static const uint8_t empty_payload = 0U;
  int enc_len;
  int len = 0;

  if (mesh == NULL || sender == NULL || dest == NULL || secret == NULL ||
      (data == NULL && data_len > 0U)) {
    return NULL;
  }

  if (type != PAYLOAD_TYPE_ANON_REQ) {
    return NULL;
  }
  if (data_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN -
                     MESHCORE_MESH_PATH_HASH_SIZE -
                     MESHCORE_PUBLIC_KEY_SIZE -
                     MESHCORE_MESH_CIPHER_BLOCK_SIZE + 1U) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  if (data == NULL) {
    data = &empty_payload;
  }

  packet->header = (uint8_t)(type << PH_TYPE_SHIFT);
  len += meshcore_identity_copy_hash_to(dest, &packet->payload[len]);
  memcpy(&packet->payload[len], sender->identity.pub_key, MESHCORE_PUBLIC_KEY_SIZE);
  len += MESHCORE_PUBLIC_KEY_SIZE;
  enc_len = meshcore_utils_encrypt_then_mac(secret, &packet->payload[len], data,
                                            (int)data_len);
  if (enc_len < 0) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return NULL;
  }
  len += enc_len;
  packet->payload_len = (uint16_t)len;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_group_datagram(
    struct meshcore_mesh *mesh, uint8_t type,
    const struct meshcore_group_channel *channel, const uint8_t *data,
    size_t data_len)
{
  struct meshcore_packet *packet;
  static const uint8_t empty_payload = 0U;
  int enc_len;
  int len = 0;

  if (mesh == NULL || channel == NULL || (data == NULL && data_len > 0U)) {
    return NULL;
  }

  if (!(type == PAYLOAD_TYPE_GRP_TXT || type == PAYLOAD_TYPE_GRP_DATA)) {
    return NULL;
  }
  if (data_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN -
                     MESHCORE_MESH_PATH_HASH_SIZE -
                     MESHCORE_MESH_CIPHER_BLOCK_SIZE + 1U) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  if (data == NULL) {
    data = &empty_payload;
  }

  packet->header = (uint8_t)(type << PH_TYPE_SHIFT);
  memcpy(&packet->payload[len], channel->hash, MESHCORE_MESH_PATH_HASH_SIZE);
  len += MESHCORE_MESH_PATH_HASH_SIZE;
  enc_len = meshcore_utils_encrypt_then_mac(channel->secret, &packet->payload[len], data,
                                            (int)data_len);
  if (enc_len < 0) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return NULL;
  }
  len += enc_len;
  packet->payload_len = (uint16_t)len;
  return packet;
}

bool meshcore_mesh_datagram_plaintext_fits(size_t len)
{
  return len <= MESHCORE_PACKET_PAYLOAD_MAX_LEN -
                    MESHCORE_MESH_CIPHER_MAC_SIZE -
                    MESHCORE_MESH_CIPHER_BLOCK_SIZE + 1U;
}

bool meshcore_mesh_path_return_extra_fits(uint8_t path_len, size_t extra_len)
{
  uint8_t path_hash_size = (uint8_t)((path_len >> 6) + 1U);
  uint8_t path_hash_count = path_len & 63U;
  size_t path_bytes = (size_t)path_hash_size * path_hash_count;

  if (path_bytes > MESHCORE_MESH_MAX_COMBINED_PATH - 5U) {
    return false;
  }
  return extra_len <= MESHCORE_MESH_MAX_COMBINED_PATH - 5U - path_bytes;
}

struct meshcore_packet *meshcore_mesh_create_ack(struct meshcore_mesh *mesh,
                                                 uint32_t ack_crc)
{
  return meshcore_mesh_create_ack_data(mesh, (const uint8_t *)&ack_crc,
                                       sizeof(ack_crc));
}

struct meshcore_packet *meshcore_mesh_create_ack_data(
    struct meshcore_mesh *mesh, const uint8_t *ack, size_t ack_len)
{
  struct meshcore_packet *packet = meshcore_mesh_obtain_new_packet(mesh);

  if (packet == NULL || (ack == NULL && ack_len > 0U) ||
      ack_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
    if (packet != NULL) {
      meshcore_mesh_release_consumed_packet(mesh, packet);
    }
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT);
  if (ack_len > 0U) {
    memcpy(packet->payload, ack, ack_len);
  }
  packet->payload_len = (uint16_t)ack_len;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_multi_ack(
    struct meshcore_mesh *mesh, uint32_t ack_crc, uint8_t remaining)
{
  return meshcore_mesh_create_multi_ack_data(mesh, (const uint8_t *)&ack_crc,
                                             sizeof(ack_crc), remaining);
}

struct meshcore_packet *meshcore_mesh_create_multi_ack_data(
    struct meshcore_mesh *mesh, const uint8_t *ack, size_t ack_len,
    uint8_t remaining)
{
  struct meshcore_packet *packet = meshcore_mesh_obtain_new_packet(mesh);

  if (packet == NULL || (ack == NULL && ack_len > 0U) ||
      ack_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN - 1U) {
    if (packet != NULL) {
      meshcore_mesh_release_consumed_packet(mesh, packet);
    }
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_MULTIPART << PH_TYPE_SHIFT);
  packet->payload[0] = (uint8_t)((remaining << 4) | PAYLOAD_TYPE_ACK);
  if (ack_len > 0U) {
    memcpy(&packet->payload[1], ack, ack_len);
  }
  packet->payload_len = (uint16_t)(1U + ack_len);
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_path_return_by_dest_hash(
    struct meshcore_mesh *mesh, const uint8_t *dest_hash, const uint8_t *secret,
    const uint8_t *path, uint8_t path_len, uint8_t extra_type,
    const uint8_t *extra, size_t extra_len)
{
  struct meshcore_packet *packet;
  uint8_t path_hash_size;
  uint8_t path_hash_count;
  int enc_len;
  int len = 0;
  uint8_t data[MESHCORE_PACKET_PAYLOAD_MAX_LEN];
  int data_len = 0;
  int path_bytes;

  if (mesh == NULL || dest_hash == NULL || secret == NULL) {
    return NULL;
  }

  path_hash_size = (uint8_t)((path_len >> 6) + 1U);
  path_hash_count = path_len & 63U;
  path_bytes = path_hash_size * path_hash_count;

  if ((path_bytes > 0 && path == NULL) || (extra_len > 0U && extra == NULL)) {
    return NULL;
  }

  if (!meshcore_mesh_path_return_extra_fits(path_len, extra_len)) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_PATH << PH_TYPE_SHIFT);
  memcpy(&packet->payload[len], dest_hash, MESHCORE_MESH_PATH_HASH_SIZE);
  len += MESHCORE_MESH_PATH_HASH_SIZE;
  len += meshcore_identity_copy_hash_to(&mesh->self_id.identity, &packet->payload[len]);

  data[data_len++] = path_len;
  if (path_bytes > 0 && path != NULL) {
    memcpy(&data[data_len], path, (size_t)path_bytes);
    data_len += path_bytes;
  }

  if (extra_len > 0U) {
    data[data_len++] = extra_type;
    memcpy(&data[data_len], extra, extra_len);
    data_len += (int)extra_len;
  } else {
    data[data_len++] = 0xFFU;
    meshcore_mesh_rng_random(mesh, &data[data_len], 4U);
    data_len += 4;
  }

  enc_len = meshcore_utils_encrypt_then_mac(secret, &packet->payload[len], data, data_len);
  if (enc_len < 0) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return NULL;
  }
  len += enc_len;
  packet->payload_len = (uint16_t)len;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_path_return_by_identity(
    struct meshcore_mesh *mesh, const struct meshcore_identity *dest,
    const uint8_t *secret, const uint8_t *path, uint8_t path_len,
    uint8_t extra_type, const uint8_t *extra, size_t extra_len)
{
  uint8_t dest_hash[MESHCORE_MESH_PATH_HASH_SIZE];

  if (dest == NULL) {
    return NULL;
  }

  meshcore_identity_copy_hash_to(dest, dest_hash);
  return meshcore_mesh_create_path_return_by_dest_hash(
      mesh, dest_hash, secret, path, path_len, extra_type, extra, extra_len);
}

struct meshcore_packet *meshcore_mesh_create_raw_data(
    struct meshcore_mesh *mesh, const uint8_t *data, size_t len)
{
  struct meshcore_packet *packet;

  if (mesh == NULL || (data == NULL && len > 0U) ||
      len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_RAW_CUSTOM << PH_TYPE_SHIFT);
  if (len > 0U) {
    memcpy(packet->payload, data, len);
  }
  packet->payload_len = (uint16_t)len;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_trace(struct meshcore_mesh *mesh,
                                                   uint32_t tag,
                                                   uint32_t auth_code,
                                                   uint8_t flags)
{
  struct meshcore_packet *packet = meshcore_mesh_obtain_new_packet(mesh);

  if (packet == NULL) {
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT);
  memcpy(packet->payload, &tag, sizeof(tag));
  memcpy(&packet->payload[4], &auth_code, sizeof(auth_code));
  packet->payload[8] = flags;
  packet->payload_len = 9U;
  return packet;
}

struct meshcore_packet *meshcore_mesh_create_control_data(
    struct meshcore_mesh *mesh, const uint8_t *data, size_t len)
{
  struct meshcore_packet *packet;

  if (mesh == NULL || (data == NULL && len > 0U) ||
      len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
    return NULL;
  }

  packet = meshcore_mesh_obtain_new_packet(mesh);
  if (packet == NULL) {
    return NULL;
  }

  packet->header = (uint8_t)(PAYLOAD_TYPE_CONTROL << PH_TYPE_SHIFT);
  if (len > 0U) {
    memcpy(packet->payload, data, len);
  }
  packet->payload_len = (uint16_t)len;
  return packet;
}

void meshcore_mesh_send_flood(struct meshcore_mesh *mesh,
                              struct meshcore_packet *packet,
                              uint32_t delay_millis, uint8_t path_hash_size)
{
  uint8_t pri;
  uint8_t type;

  if (mesh == NULL || packet == NULL) {
    return;
  }

  type = meshcore_packet_get_payload_type(packet);
  if (type == PAYLOAD_TYPE_TRACE || path_hash_size == 0U || path_hash_size > 3U) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return;
  }

  packet->header &= (uint8_t)~PH_ROUTE_MASK;
  packet->header |= ROUTE_TYPE_FLOOD;
  meshcore_packet_set_path_hash_size_and_count(packet, path_hash_size, 0U);
  (void)meshcore_mesh_tables_has_seen(mesh, packet);

  if (type == PAYLOAD_TYPE_PATH) {
    pri = 2U;
  } else if (type == PAYLOAD_TYPE_ADVERT) {
    pri = 3U;
  } else {
    pri = 1U;
  }

  meshcore_dispatcher_send_packet(&mesh->dispatcher, packet, pri, delay_millis);
}

void meshcore_mesh_send_flood_by_transport_codes(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint16_t transport_codes[2], uint32_t delay_millis,
    uint8_t path_hash_size)
{
  uint8_t pri;
  uint8_t type;
  uint8_t hash_size;

  if (mesh == NULL || packet == NULL || transport_codes == NULL) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return;
  }

  type = meshcore_packet_get_payload_type(packet);
  if (type == PAYLOAD_TYPE_TRACE || path_hash_size == 0U || path_hash_size > 3U) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return;
  }

  packet->header &= (uint8_t)~PH_ROUTE_MASK;
  packet->header |= ROUTE_TYPE_TRANSPORT_FLOOD;
  packet->transport_codes[0] = transport_codes[0];
  packet->transport_codes[1] = transport_codes[1];
  hash_size = meshcore_mesh_transport_flood_hash_size(path_hash_size, transport_codes);
  meshcore_packet_set_path_hash_size_and_count(packet, hash_size, 0U);
  (void)meshcore_mesh_tables_has_seen(mesh, packet);

  if (type == PAYLOAD_TYPE_PATH) {
    pri = 2U;
  } else if (type == PAYLOAD_TYPE_ADVERT) {
    pri = 3U;
  } else {
    pri = 1U;
  }

  meshcore_dispatcher_send_packet(&mesh->dispatcher, packet, pri, delay_millis);
}

void meshcore_mesh_send_direct(struct meshcore_mesh *mesh,
                               struct meshcore_packet *packet,
                               const uint8_t *path, uint8_t path_len,
                               uint32_t delay_millis)
{
  uint8_t type;
  uint8_t pri;

  if (mesh == NULL || packet == NULL || (path == NULL && path_len > 0U)) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return;
  }

  packet->header &= (uint8_t)~PH_ROUTE_MASK;
  packet->header |= ROUTE_TYPE_DIRECT;
  type = meshcore_packet_get_payload_type(packet);

  if (type == PAYLOAD_TYPE_TRACE) {
    if ((size_t)packet->payload_len + path_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
      meshcore_mesh_release_consumed_packet(mesh, packet);
      return;
    }
    memcpy(&packet->payload[packet->payload_len], path, path_len);
    packet->payload_len += path_len;
    packet->path_len = 0U;
    pri = 5U;
  } else {
    packet->path_len = meshcore_packet_copy_path(packet->path, path, path_len);
    pri = (type == PAYLOAD_TYPE_PATH) ? 1U : 0U;
  }

  (void)meshcore_mesh_tables_has_seen(mesh, packet);
  meshcore_dispatcher_send_packet(&mesh->dispatcher, packet, pri, delay_millis);
}

void meshcore_mesh_send_zero_hop(struct meshcore_mesh *mesh,
                                 struct meshcore_packet *packet,
                                 uint32_t delay_millis)
{
  if (mesh == NULL || packet == NULL) {
    return;
  }

  packet->header &= (uint8_t)~PH_ROUTE_MASK;
  packet->header |= ROUTE_TYPE_DIRECT;
  packet->path_len = 0U;
  (void)meshcore_mesh_tables_has_seen(mesh, packet);
  meshcore_dispatcher_send_packet(&mesh->dispatcher, packet, 0U, delay_millis);
}

void meshcore_mesh_send_zero_hop_by_transport_codes(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint16_t transport_codes[2], uint32_t delay_millis)
{
  if (mesh == NULL || packet == NULL || transport_codes == NULL) {
    meshcore_mesh_release_consumed_packet(mesh, packet);
    return;
  }

  packet->header &= (uint8_t)~PH_ROUTE_MASK;
  packet->header |= ROUTE_TYPE_TRANSPORT_DIRECT;
  packet->transport_codes[0] = transport_codes[0];
  packet->transport_codes[1] = transport_codes[1];
  packet->path_len = 0U;
  (void)meshcore_mesh_tables_has_seen(mesh, packet);
  meshcore_dispatcher_send_packet(&mesh->dispatcher, packet, 0U, delay_millis);
}

bool meshcore_mesh_runtime_filter_recv_flood_packet(struct meshcore_mesh *mesh,
                                            struct meshcore_packet *packet)
{
  (void)mesh;
  return meshcore_platform_bridge_mesh_filter_recv_flood_packet(packet);
}

bool meshcore_mesh_runtime_allow_packet_forward(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->allow_packet_forward != NULL) {
    return mesh->runtime_ops->allow_packet_forward(mesh->runtime_user_data, mesh,
                                                   packet);
  }

  return meshcore_platform_bridge_mesh_allow_packet_forward(packet);
}

uint32_t meshcore_mesh_runtime_get_retransmit_delay(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->get_retransmit_delay != NULL) {
    return mesh->runtime_ops->get_retransmit_delay(mesh->runtime_user_data, mesh,
                                                   packet);
  }

  return meshcore_platform_bridge_mesh_retransmit_delay_get(packet);
}

uint32_t meshcore_mesh_runtime_get_direct_retransmit_delay(
    struct meshcore_mesh *mesh, const struct meshcore_packet *packet)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->get_direct_retransmit_delay != NULL) {
    return mesh->runtime_ops->get_direct_retransmit_delay(
        mesh->runtime_user_data, mesh, packet);
  }

  return meshcore_platform_bridge_mesh_direct_retransmit_delay_get(packet);
}

uint8_t meshcore_mesh_runtime_get_extra_ack_transmit_count(struct meshcore_mesh *mesh)
{
  (void)mesh;
  return meshcore_platform_bridge_mesh_extra_ack_transmit_count_get();
}

int meshcore_mesh_runtime_next_peer_shared_secret_by_hash(
    struct meshcore_mesh *mesh, const uint8_t *hash, size_t start_slot,
    size_t *slot_id, uint8_t *dest_secret,
    struct meshcore_common_peer_identity *peer_identity)
{
  (void)mesh;
  return meshcore_platform_bridge_mesh_next_peer_shared_secret_by_hash(hash, start_slot, slot_id,
                                                       dest_secret, peer_identity);
}

void meshcore_mesh_runtime_on_peer_data_recv(struct meshcore_mesh *mesh,
                                     struct meshcore_packet *packet,
                                     uint8_t type,
                                     const struct meshcore_common_peer_identity *sender,
                                     const uint8_t *secret, uint8_t *data,
                                     size_t len)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_peer_data_recv != NULL) {
    mesh->runtime_ops->on_peer_data_recv(mesh->runtime_user_data, mesh, packet,
                                         type, sender, secret, data, len);
    return;
  }

  meshcore_platform_bridge_mesh_on_peer_data_recv(packet, type, sender, secret, data, len);
}

void meshcore_mesh_runtime_on_trace_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet, uint32_t tag,
    uint32_t auth_code, uint8_t flags, const uint8_t *path_snrs,
    const uint8_t *path_hashes, uint8_t path_len)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_trace_recv != NULL) {
    mesh->runtime_ops->on_trace_recv(mesh->runtime_user_data, mesh, packet, tag,
                                     auth_code, flags, path_snrs, path_hashes,
                                     path_len);
    return;
  }

  meshcore_platform_bridge_mesh_on_trace_recv(packet, tag, auth_code, flags, path_snrs,
                              path_hashes, path_len);
}

bool meshcore_mesh_runtime_on_peer_path_recv(struct meshcore_mesh *mesh,
                                     struct meshcore_packet *packet,
                                     const struct meshcore_common_peer_identity *sender,
                                     const uint8_t *secret,
                                     uint8_t *path, uint8_t path_len,
                                     uint8_t extra_type, uint8_t *extra,
                                     uint8_t extra_len)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_peer_path_recv != NULL) {
    return mesh->runtime_ops->on_peer_path_recv(
        mesh->runtime_user_data, mesh, packet, sender, secret, path, path_len,
        extra_type, extra, extra_len);
  }

  return meshcore_platform_bridge_mesh_on_peer_path_recv(packet, sender, secret, path, path_len,
                                         extra_type, extra, extra_len);
}

void meshcore_mesh_runtime_on_advert_recv(struct meshcore_mesh *mesh,
                                  struct meshcore_packet *packet,
                                  const struct meshcore_identity *identity,
                                  uint32_t timestamp, const uint8_t *app_data,
                                  size_t app_data_len)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_advert_recv != NULL) {
    mesh->runtime_ops->on_advert_recv(mesh->runtime_user_data, mesh, packet,
                                      identity, timestamp, app_data,
                                      app_data_len);
    return;
  }

  meshcore_platform_bridge_mesh_on_advert_recv(packet, identity, timestamp, app_data,
                                   app_data_len);
}

void meshcore_mesh_runtime_on_anon_data_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const uint8_t *secret, const struct meshcore_identity *sender,
    uint8_t *data, size_t len)
{
  (void)mesh;
  meshcore_platform_bridge_mesh_on_anon_data_recv(packet, secret, sender, data, len);
}

void meshcore_mesh_runtime_on_path_recv(struct meshcore_mesh *mesh,
                                struct meshcore_packet *packet,
                                struct meshcore_identity *sender,
                                uint8_t *path, uint8_t path_len,
                                uint8_t extra_type, uint8_t *extra,
                                uint8_t extra_len)
{
  (void)mesh;
  meshcore_platform_bridge_mesh_on_path_recv(packet, sender, path, path_len, extra_type,
                                 extra, extra_len);
}

void meshcore_mesh_runtime_on_control_data_recv(struct meshcore_mesh *mesh,
                                        struct meshcore_packet *packet)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_control_data_recv != NULL) {
    mesh->runtime_ops->on_control_data_recv(mesh->runtime_user_data, mesh,
                                            packet);
    return;
  }

  meshcore_platform_bridge_mesh_on_control_data_recv(packet);
}

void meshcore_mesh_runtime_on_raw_data_recv(struct meshcore_mesh *mesh,
                                            struct meshcore_packet *packet)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_raw_data_recv != NULL) {
    mesh->runtime_ops->on_raw_data_recv(mesh->runtime_user_data, mesh, packet);
    return;
  }

  meshcore_platform_bridge_mesh_on_raw_data_recv(packet);
}

int meshcore_mesh_runtime_search_channels_by_hash(
    struct meshcore_mesh *mesh, const uint8_t *hash,
    struct meshcore_group_channel channels[], int max_matches)
{
  (void)mesh;
  return meshcore_platform_bridge_mesh_search_channels_by_hash(hash, channels, max_matches);
}

void meshcore_mesh_runtime_on_group_data_recv(
    struct meshcore_mesh *mesh, struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data,
    size_t len)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_group_data_recv != NULL) {
    mesh->runtime_ops->on_group_data_recv(mesh->runtime_user_data, mesh, packet,
                                          type, channel, data, len);
    return;
  }

  meshcore_platform_bridge_mesh_on_group_data_recv(packet, type, channel, data, len);
}

void meshcore_mesh_runtime_on_ack_recv(struct meshcore_mesh *mesh,
                               struct meshcore_packet *packet,
                               uint32_t ack_crc)
{
  if (mesh != NULL && mesh->runtime_ops != NULL &&
      mesh->runtime_ops->on_ack_recv != NULL) {
    mesh->runtime_ops->on_ack_recv(mesh->runtime_user_data, mesh, packet,
                                   ack_crc);
    return;
  }

  meshcore_platform_bridge_mesh_on_ack_recv(packet, ack_crc);
}
