// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <string.h>

#include "meshcore_identity.h"
#include "meshcore_mesh.h"
#include "meshcore_packet.h"
#include "meshcore_tables.h"
#include "meshcore_utils.h"

static int test_utils_from_hex_rejects_invalid_input(void)
{
  uint8_t out[2] = {0xAAU, 0xAAU};

  NATIVE_TEST_ASSERT(meshcore_utils_from_hex(out, sizeof(out), "00ff"));
  NATIVE_TEST_ASSERT_EQ(0x00U, out[0]);
  NATIVE_TEST_ASSERT_EQ(0xFFU, out[1]);

  out[0] = 0xAAU;
  out[1] = 0xAAU;
  NATIVE_TEST_ASSERT(!meshcore_utils_from_hex(out, sizeof(out), "00gg"));
  NATIVE_TEST_ASSERT(!meshcore_utils_from_hex(NULL, sizeof(out), "00ff"));
  NATIVE_TEST_ASSERT(!meshcore_utils_from_hex(out, -1, "00ff"));
  NATIVE_TEST_ASSERT(!meshcore_utils_from_hex(out, sizeof(out), NULL));

  return 0;
}

static int test_identity_known_private_key_validates_and_signs(void)
{
  static const uint8_t test_client_prv[MESHCORE_PRIVATE_KEY_SIZE] = {
    0x70, 0x65, 0xe1, 0x8f, 0xd9, 0xfa, 0xbb, 0x70,
    0xc1, 0xed, 0x90, 0xdc, 0xa1, 0x99, 0x07, 0xde,
    0x69, 0x8c, 0x88, 0xb7, 0x09, 0xea, 0x14, 0x6e,
    0xaf, 0xd9, 0x3d, 0x9b, 0x83, 0x0c, 0x7b, 0x60,
    0xc4, 0x68, 0x11, 0x93, 0xc7, 0x9b, 0xbc, 0x39,
    0x94, 0x5b, 0xa8, 0x06, 0x41, 0x04, 0xbb, 0x61,
    0x8f, 0x8f, 0xd7, 0xa8, 0x4a, 0x0a, 0xf6, 0xf5,
    0x70, 0x33, 0xd6, 0xe8, 0xdd, 0xcd, 0x64, 0x71
  };
  static const uint8_t test_client_pub[MESHCORE_PUBLIC_KEY_SIZE] = {
    0x1e, 0xc7, 0x71, 0x75, 0xb0, 0x91, 0x8e, 0xd2,
    0x06, 0xf9, 0xae, 0x04, 0xec, 0x13, 0x6d, 0x6d,
    0x5d, 0x43, 0x15, 0xbb, 0x26, 0x30, 0x54, 0x27,
    0xf6, 0x45, 0xb4, 0x92, 0xe9, 0x35, 0x0c, 0x10
  };
  static const uint8_t message[] = {
    0x4d, 0x65, 0x73, 0x68, 0x43, 0x6f, 0x72, 0x65
  };
  struct meshcore_local_identity local;
  struct meshcore_identity verifier;
  uint8_t signature[64];
  uint8_t tampered[sizeof(message)];

  NATIVE_TEST_ASSERT(meshcore_local_identity_validate_private_key(test_client_prv));

  meshcore_local_identity_init_from_bytes(&local, test_client_prv,
                                          test_client_pub);
  meshcore_identity_init_from_pub_key(&verifier, test_client_pub);
  meshcore_local_identity_sign(&local, signature, message,
                               (int)sizeof(message));
  NATIVE_TEST_ASSERT(meshcore_identity_verify(&verifier, signature, message,
                                             (int)sizeof(message)));

  memcpy(tampered, message, sizeof(tampered));
  tampered[0] ^= 0x01U;
  NATIVE_TEST_ASSERT(!meshcore_identity_verify(&verifier, signature, tampered,
                                              (int)sizeof(tampered)));

  return 0;
}

static int test_tables_hash_ack_packets_by_full_payload(void)
{
  struct meshcore_tables tables;
  struct meshcore_packet packet;
  uint8_t ack[] = {0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU};

  meshcore_tables_init(&tables);
  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  memcpy(packet.payload, ack, sizeof(ack));
  packet.payload_len = sizeof(ack);

  NATIVE_TEST_ASSERT(!meshcore_tables_has_seen(&tables, &packet));
  NATIVE_TEST_ASSERT_EQ(1, tables.next_idx);
  NATIVE_TEST_ASSERT(meshcore_tables_has_seen(&tables, &packet));
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_tables_get_num_direct_dups(&tables));

  ack[5] ^= 0x01U;
  memcpy(packet.payload, ack, sizeof(ack));
  NATIVE_TEST_ASSERT(!meshcore_tables_has_seen(&tables, &packet));
  NATIVE_TEST_ASSERT_EQ(2, tables.next_idx);

  meshcore_tables_clear(&tables, &packet);
  NATIVE_TEST_ASSERT(!meshcore_tables_has_seen(&tables, &packet));

  return 0;
}

static int test_mesh_peer_datagram_boundary_matches_upstream_precheck(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_tables tables;
  struct meshcore_mesh mesh;
  struct meshcore_identity dest;
  struct meshcore_packet *packet;
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t self_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t peer_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t data[MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN + sizeof(uint32_t)];
  uint8_t oversized[sizeof(data) + 1U];
  size_t i;

  for (i = 0U; i < sizeof(secret); i++) {
    secret[i] = (uint8_t)(0x40U + i);
    self_key[i] = (uint8_t)(0x80U + i);
    peer_key[i] = (uint8_t)(0xC0U + i);
  }
  memset(data, 0x11, sizeof(data));
  memset(oversized, 0x22, sizeof(oversized));

  meshcore_packet_queue_manager_prepare(&manager, 2);
  meshcore_tables_init(&tables);
  meshcore_mesh_init(&mesh, &manager, &tables);
  meshcore_identity_init_from_pub_key(&mesh.self_id.identity, self_key);
  meshcore_identity_init_from_pub_key(&dest, peer_key);

  packet = meshcore_mesh_create_datagram(&mesh, PAYLOAD_TYPE_RESPONSE, &dest,
                                         secret, data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT(packet->payload_len <= MESHCORE_PACKET_PAYLOAD_MAX_LEN);

  packet = meshcore_mesh_create_datagram(&mesh, PAYLOAD_TYPE_RESPONSE, &dest,
                                         secret, oversized, sizeof(oversized));
  NATIVE_TEST_ASSERT(packet == NULL);

  meshcore_packet_queue_manager_deinit(&manager);
  return 0;
}

static int test_mesh_ack_helpers_preserve_payload_length(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_tables tables;
  struct meshcore_mesh mesh;
  struct meshcore_packet *packet;
  uint8_t ack[] = {0x10U, 0x20U, 0x30U, 0x40U, 0x50U, 0x60U};

  meshcore_packet_queue_manager_prepare(&manager, 3);
  meshcore_tables_init(&tables);
  meshcore_mesh_init(&mesh, &manager, &tables);

  packet = meshcore_mesh_create_ack_data(&mesh, ack, sizeof(ack));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ACK, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(sizeof(ack), packet->payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet->payload, ack, sizeof(ack)) == 0);

  packet = meshcore_mesh_create_multi_ack_data(&mesh, ack, sizeof(ack), 1U);
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_MULTIPART,
                        meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(1U + sizeof(ack), packet->payload_len);
  NATIVE_TEST_ASSERT_EQ((1U << 4) | PAYLOAD_TYPE_ACK, packet->payload[0]);
  NATIVE_TEST_ASSERT(memcmp(&packet->payload[1], ack, sizeof(ack)) == 0);

  meshcore_packet_queue_manager_deinit(&manager);
  return 0;
}

int main(void)
{
  struct meshcore_packet source;
  struct meshcore_packet parsed;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t len;
  const uint8_t payload[] = {0x10U, 0x20U, 0x30U, 0x40U};

  meshcore_packet_init(&source);
  source.header = (uint8_t)((MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM
                             << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  source.path_len = 0U;
  memcpy(source.payload, payload, sizeof(payload));
  source.payload_len = sizeof(payload);

  len = meshcore_packet_write_to(&source, raw);
  NATIVE_TEST_ASSERT_EQ(6U, len);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&parsed, raw, len));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM,
                        meshcore_packet_get_payload_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(sizeof(payload), parsed.payload_len);
  NATIVE_TEST_ASSERT(memcmp(parsed.payload, payload, sizeof(payload)) == 0);

  NATIVE_TEST_ASSERT_EQ(0, test_utils_from_hex_rejects_invalid_input());
  NATIVE_TEST_ASSERT_EQ(0, test_identity_known_private_key_validates_and_signs());
  NATIVE_TEST_ASSERT_EQ(0, test_tables_hash_ack_packets_by_full_payload());
  NATIVE_TEST_ASSERT_EQ(0, test_mesh_peer_datagram_boundary_matches_upstream_precheck());
  NATIVE_TEST_ASSERT_EQ(0, test_mesh_ack_helpers_preserve_payload_length());

  return 0;
}
