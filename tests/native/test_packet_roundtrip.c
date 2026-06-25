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

/*
 * Layer 1 parity evidence:
 * - .reference/meshcore/src/Utils.h
 * - .reference/meshcore/src/Utils.cpp
 * - .reference/meshcore/src/Identity.h
 * - .reference/meshcore/src/Identity.cpp
 * - .reference/meshcore/src/Mesh.h
 * - .reference/meshcore/src/Mesh.cpp
 * - .reference/meshcore/src/helpers/SimpleMeshTables.h
 */

static void fake_digest_expected(uint8_t *dest, size_t dest_len,
                                 const uint8_t *data, size_t data_len)
{
  size_t i;
  uint8_t acc = 0x5AU;

  for (i = 0U; i < data_len; i++) {
    acc = (uint8_t)(acc + data[i] + (uint8_t)i);
  }
  for (i = 0U; i < dest_len; i++) {
    dest[i] = (uint8_t)(acc + i);
  }
}

static void fill_incrementing(uint8_t *dest, size_t len, uint8_t base)
{
  size_t i;

  for (i = 0U; i < len; i++) {
    dest[i] = (uint8_t)(base + i);
  }
}

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

static int test_utils_crypto_and_text_helpers_match_promoted_contract(void)
{
  const uint8_t msg[] = {0x01U, 0x02U, 0x03U};
  const uint8_t frag1[] = {0x10U, 0x11U};
  const uint8_t frag2[] = {0x20U, 0x21U, 0x22U};
  const uint8_t plain[] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t hash[8];
  uint8_t expected_hash[8];
  uint8_t combined[sizeof(frag1) + sizeof(frag2)];
  uint8_t encrypted[32];
  uint8_t decrypted[32];
  char hex[7];
  char text[] = "alpha,beta,gamma";
  const char *parts[3] = {0};
  int enc_len;
  int dec_len;

  fill_incrementing(secret, sizeof(secret), 0x30U);

  meshcore_utils_sha256(hash, sizeof(hash), msg, (int)sizeof(msg));
  fake_digest_expected(expected_hash, sizeof(expected_hash), msg, sizeof(msg));
  NATIVE_TEST_ASSERT(memcmp(hash, expected_hash, sizeof(hash)) == 0);

  memcpy(combined, frag1, sizeof(frag1));
  memcpy(&combined[sizeof(frag1)], frag2, sizeof(frag2));
  meshcore_utils_sha256_two_fragments(hash, sizeof(hash), frag1,
                                      (int)sizeof(frag1), frag2,
                                      (int)sizeof(frag2));
  fake_digest_expected(expected_hash, sizeof(expected_hash), combined,
                       sizeof(combined));
  NATIVE_TEST_ASSERT(memcmp(hash, expected_hash, sizeof(hash)) == 0);

  enc_len = meshcore_utils_encrypt(secret, encrypted, plain,
                                   (int)sizeof(plain));
  NATIVE_TEST_ASSERT_EQ(16, enc_len);
  NATIVE_TEST_ASSERT(memcmp(encrypted, plain, sizeof(plain)) == 0);
  NATIVE_TEST_ASSERT_EQ(0U, encrypted[sizeof(plain)]);

  dec_len = meshcore_utils_decrypt(secret, decrypted, encrypted, enc_len);
  NATIVE_TEST_ASSERT_EQ(enc_len, dec_len);
  NATIVE_TEST_ASSERT(memcmp(decrypted, plain, sizeof(plain)) == 0);

  enc_len = meshcore_utils_encrypt_then_mac(secret, encrypted, plain,
                                            (int)sizeof(plain));
  NATIVE_TEST_ASSERT_EQ(18, enc_len);
  dec_len = meshcore_utils_mac_then_decrypt(secret, decrypted, encrypted,
                                            enc_len);
  NATIVE_TEST_ASSERT_EQ(16, dec_len);
  NATIVE_TEST_ASSERT(memcmp(decrypted, plain, sizeof(plain)) == 0);
  encrypted[0] ^= 0x01U;
  NATIVE_TEST_ASSERT_EQ(0, meshcore_utils_mac_then_decrypt(secret, decrypted,
                                                           encrypted, enc_len));

  meshcore_utils_to_hex(hex, msg, sizeof(msg));
  NATIVE_TEST_ASSERT(strcmp(hex, "010203") == 0);
  NATIVE_TEST_ASSERT(meshcore_utils_is_hex_char('0'));
  NATIVE_TEST_ASSERT(meshcore_utils_is_hex_char('F'));
  NATIVE_TEST_ASSERT(!meshcore_utils_is_hex_char('g'));

  NATIVE_TEST_ASSERT_EQ(3, meshcore_utils_parse_text_parts(text, parts, 3, ','));
  NATIVE_TEST_ASSERT(strcmp(parts[0], "alpha") == 0);
  NATIVE_TEST_ASSERT(strcmp(parts[1], "beta") == 0);
  NATIVE_TEST_ASSERT(strcmp(parts[2], "gamma") == 0);

  return 0;
}

static int test_packet_hash_includes_payload_type_and_trace_path_len(void)
{
  struct meshcore_packet packet;
  uint8_t hash[MESHCORE_PACKET_HASH_SIZE];
  uint8_t expected[MESHCORE_PACKET_HASH_SIZE];
  uint8_t data[1U + sizeof(uint16_t) + 3U];

  meshcore_packet_init(&packet);
  packet.header = (uint8_t)(PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  packet.payload[0] = 0xAAU;
  packet.payload[1] = 0xBBU;
  packet.payload[2] = 0xCCU;
  packet.payload_len = 3U;
  meshcore_packet_calculate_hash(&packet, hash);
  data[0] = PAYLOAD_TYPE_TXT_MSG;
  memcpy(&data[1], packet.payload, packet.payload_len);
  fake_digest_expected(expected, sizeof(expected), data, 4U);
  NATIVE_TEST_ASSERT(memcmp(hash, expected, sizeof(hash)) == 0);

  packet.header = (uint8_t)(PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT);
  packet.path_len = 0x42U;
  meshcore_packet_calculate_hash(&packet, hash);
  data[0] = PAYLOAD_TYPE_TRACE;
  memcpy(&data[1], &packet.path_len, sizeof(packet.path_len));
  memcpy(&data[1U + sizeof(packet.path_len)], packet.payload,
         packet.payload_len);
  fake_digest_expected(expected, sizeof(expected), data,
                       1U + sizeof(packet.path_len) + packet.payload_len);
  NATIVE_TEST_ASSERT(memcmp(hash, expected, sizeof(hash)) == 0);

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

static int test_identity_hex_hash_shared_secret_and_storage_roundtrip(void)
{
  static const char pub_hex[] =
      "1ec77175b0918ed206f9ae04ec136d6d5d4315bb26305427f645b492e9350c10";
  static const char prv_hex[] =
      "7065e18fd9fabb70c1ed90dca19907de698c88b709ea146eafd93d9b830c7b60"
      "c4681193c79bbc39945ba8064104bb618f8fd7a84a0af6f57033d6e8ddcd6471";
  struct meshcore_identity identity;
  struct meshcore_identity other;
  struct meshcore_local_identity local;
  struct meshcore_local_identity loaded;
  uint8_t hash[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t shared1[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t shared2[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t storage[MESHCORE_PRIVATE_KEY_SIZE + MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t other_pub[MESHCORE_PUBLIC_KEY_SIZE];
  size_t written;

  fill_incrementing(other_pub, sizeof(other_pub), 0x31U);

  NATIVE_TEST_ASSERT(meshcore_identity_init_from_pub_hex(&identity, pub_hex));
  NATIVE_TEST_ASSERT(!meshcore_identity_init_from_pub_hex(&other, "00"));
  NATIVE_TEST_ASSERT(meshcore_local_identity_init_from_hex(&local, prv_hex,
                                                           pub_hex));
  meshcore_identity_init_from_pub_key(&other, other_pub);

  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_HASH_BYTES,
                        meshcore_identity_copy_hash_to(&identity, hash));
  NATIVE_TEST_ASSERT(meshcore_identity_is_hash_match(&identity, hash));
  hash[0] ^= 0x01U;
  NATIVE_TEST_ASSERT(!meshcore_identity_is_hash_match(&identity, hash));

  NATIVE_TEST_ASSERT(meshcore_identity_matches(&identity, &local.identity));
  NATIVE_TEST_ASSERT(meshcore_identity_matches_by_pub_key(&identity,
                                                          local.identity.pub_key));
  NATIVE_TEST_ASSERT(!meshcore_identity_matches(&identity, &other));

  meshcore_local_identity_calc_shared_secret(&local, shared1, other.pub_key);
  meshcore_local_identity_calc_shared_secret(&local, shared2, other.pub_key);
  NATIVE_TEST_ASSERT(memcmp(shared1, shared2, sizeof(shared1)) == 0);

  written = meshcore_local_identity_write_to(&local, storage, sizeof(storage));
  NATIVE_TEST_ASSERT_EQ(sizeof(storage), written);
  meshcore_local_identity_init(&loaded);
  meshcore_local_identity_read_from(&loaded, storage, written);
  NATIVE_TEST_ASSERT(meshcore_identity_matches(&identity, &loaded.identity));

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

static int test_mesh_packet_builders_cover_required_payload_types(void)
{
  struct meshcore_packet_queue_manager manager;
  struct meshcore_tables tables;
  struct meshcore_mesh mesh;
  struct meshcore_identity dest;
  struct meshcore_group_channel channel;
  struct meshcore_packet *packet;
  uint8_t self_prv[MESHCORE_PRIVATE_KEY_SIZE];
  uint8_t self_pub[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t dest_pub[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t data[8];
  uint8_t path[] = {0x44U, 0x55U};
  uint8_t path_len = (uint8_t)((1U << 6) | 1U);
  uint8_t extra[] = {0x9AU, 0x9BU};

  fill_incrementing(self_prv, sizeof(self_prv), 0x10U);
  fill_incrementing(self_pub, sizeof(self_pub), 0x60U);
  fill_incrementing(dest_pub, sizeof(dest_pub), 0xA0U);
  fill_incrementing(secret, sizeof(secret), 0x20U);
  fill_incrementing(data, sizeof(data), 0x33U);
  fill_incrementing(channel.hash, sizeof(channel.hash), 0x77U);
  memcpy(channel.secret, secret, sizeof(channel.secret));

  meshcore_packet_queue_manager_prepare(&manager, 10);
  meshcore_tables_init(&tables);
  meshcore_mesh_init(&mesh, &manager, &tables);
  meshcore_local_identity_init_from_bytes(&mesh.self_id, self_prv, self_pub);
  meshcore_identity_init_from_pub_key(&dest, dest_pub);

  packet = meshcore_mesh_create_advert(&mesh, &mesh.self_id, data, 3U);
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ADVERT, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_PUBLIC_KEY_SIZE + 4U + 64U + 3U,
                        packet->payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet->payload, self_pub, sizeof(self_pub)) == 0);

  packet = meshcore_mesh_create_datagram(&mesh, PAYLOAD_TYPE_REQ, &dest,
                                         secret, data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_REQ, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(MESHCORE_CHANNEL_HASH_BYTES * 2U + 2U + 16U,
                        packet->payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet->payload, dest_pub,
                            MESHCORE_CHANNEL_HASH_BYTES) == 0);
  NATIVE_TEST_ASSERT(memcmp(&packet->payload[MESHCORE_CHANNEL_HASH_BYTES],
                            self_pub, MESHCORE_CHANNEL_HASH_BYTES) == 0);

  packet = meshcore_mesh_create_anon_datagram(&mesh, PAYLOAD_TYPE_ANON_REQ,
                                              &mesh.self_id, &dest, secret,
                                              data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ANON_REQ,
                        meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT(memcmp(packet->payload, dest_pub,
                            MESHCORE_CHANNEL_HASH_BYTES) == 0);
  NATIVE_TEST_ASSERT(memcmp(&packet->payload[MESHCORE_CHANNEL_HASH_BYTES],
                            self_pub, MESHCORE_PUBLIC_KEY_SIZE) == 0);

  packet = meshcore_mesh_create_group_datagram(&mesh, PAYLOAD_TYPE_GRP_DATA,
                                               &channel, data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_DATA,
                        meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT(memcmp(packet->payload, channel.hash,
                            sizeof(channel.hash)) == 0);

  NATIVE_TEST_ASSERT(meshcore_mesh_path_return_extra_fits(path_len,
                                                          sizeof(extra)));
  packet = meshcore_mesh_create_path_return_by_identity(
      &mesh, &dest, secret, path, path_len, PAYLOAD_TYPE_RESPONSE, extra,
      sizeof(extra));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_PATH, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT(memcmp(packet->payload, dest_pub,
                            MESHCORE_CHANNEL_HASH_BYTES) == 0);

  packet = meshcore_mesh_create_trace(&mesh, 0x11223344U, 0xAABBCCDDU, 0x05U);
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TRACE, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(9U, packet->payload_len);
  NATIVE_TEST_ASSERT_EQ(0x05U, packet->payload[8]);

  packet = meshcore_mesh_create_raw_data(&mesh, data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RAW_CUSTOM,
                        meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(sizeof(data), packet->payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet->payload, data, sizeof(data)) == 0);

  packet = meshcore_mesh_create_control_data(&mesh, data, sizeof(data));
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_CONTROL,
                        meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(sizeof(data), packet->payload_len);
  NATIVE_TEST_ASSERT(memcmp(packet->payload, data, sizeof(data)) == 0);

  NATIVE_TEST_ASSERT(meshcore_mesh_create_raw_data(
      &mesh, data, MESHCORE_PACKET_PAYLOAD_MAX_LEN + 1U) == NULL);
  NATIVE_TEST_ASSERT(meshcore_mesh_create_control_data(
      &mesh, data, MESHCORE_PACKET_PAYLOAD_MAX_LEN + 1U) == NULL);

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
  NATIVE_TEST_ASSERT_EQ(0, test_utils_crypto_and_text_helpers_match_promoted_contract());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_hash_includes_payload_type_and_trace_path_len());
  NATIVE_TEST_ASSERT_EQ(0, test_identity_known_private_key_validates_and_signs());
  NATIVE_TEST_ASSERT_EQ(0, test_identity_hex_hash_shared_secret_and_storage_roundtrip());
  NATIVE_TEST_ASSERT_EQ(0, test_tables_hash_ack_packets_by_full_payload());
  NATIVE_TEST_ASSERT_EQ(0, test_mesh_peer_datagram_boundary_matches_upstream_precheck());
  NATIVE_TEST_ASSERT_EQ(0, test_mesh_ack_helpers_preserve_payload_length());
  NATIVE_TEST_ASSERT_EQ(0, test_mesh_packet_builders_cover_required_payload_types());

  return 0;
}
