#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 FoBE Studio

"""Compile locked upstream helpers and compare their behavior with meshcore-C."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import textwrap
from pathlib import Path


ORACLE_SOURCE = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Packet.h"
#include "helpers/AdvertDataHelpers.h"
#include "helpers/TxtDataHelpers.h"

enum {
  UP_PH_TYPE_SHIFT = PH_TYPE_SHIFT,
  UP_ROUTE_TYPE_TRANSPORT_DIRECT = ROUTE_TYPE_TRANSPORT_DIRECT,
  UP_PAYLOAD_TYPE_REQ = PAYLOAD_TYPE_REQ,
  UP_PAYLOAD_TYPE_RESPONSE = PAYLOAD_TYPE_RESPONSE,
  UP_PAYLOAD_TYPE_TXT_MSG = PAYLOAD_TYPE_TXT_MSG,
  UP_PAYLOAD_TYPE_ACK = PAYLOAD_TYPE_ACK,
  UP_PAYLOAD_TYPE_ADVERT = PAYLOAD_TYPE_ADVERT,
  UP_PAYLOAD_TYPE_GRP_TXT = PAYLOAD_TYPE_GRP_TXT,
  UP_PAYLOAD_TYPE_GRP_DATA = PAYLOAD_TYPE_GRP_DATA,
  UP_PAYLOAD_TYPE_ANON_REQ = PAYLOAD_TYPE_ANON_REQ,
  UP_PAYLOAD_TYPE_PATH = PAYLOAD_TYPE_PATH,
  UP_PAYLOAD_TYPE_TRACE = PAYLOAD_TYPE_TRACE,
  UP_PAYLOAD_TYPE_MULTIPART = PAYLOAD_TYPE_MULTIPART,
  UP_PAYLOAD_TYPE_CONTROL = PAYLOAD_TYPE_CONTROL,
  UP_PAYLOAD_TYPE_RAW_CUSTOM = PAYLOAD_TYPE_RAW_CUSTOM,
  UP_ADV_TYPE_CHAT = ADV_TYPE_CHAT,
  UP_TXT_TYPE_PLAIN = TXT_TYPE_PLAIN,
  UP_TXT_TYPE_CLI_DATA = TXT_TYPE_CLI_DATA,
  UP_TXT_TYPE_SIGNED_PLAIN = TXT_TYPE_SIGNED_PLAIN,
};

#undef PH_ROUTE_MASK
#undef PH_TYPE_SHIFT
#undef PH_TYPE_MASK
#undef PH_VER_SHIFT
#undef PH_VER_MASK
#undef ROUTE_TYPE_TRANSPORT_FLOOD
#undef ROUTE_TYPE_FLOOD
#undef ROUTE_TYPE_DIRECT
#undef ROUTE_TYPE_TRANSPORT_DIRECT
#undef PAYLOAD_TYPE_REQ
#undef PAYLOAD_TYPE_RESPONSE
#undef PAYLOAD_TYPE_TXT_MSG
#undef PAYLOAD_TYPE_ACK
#undef PAYLOAD_TYPE_ADVERT
#undef PAYLOAD_TYPE_GRP_TXT
#undef PAYLOAD_TYPE_GRP_DATA
#undef PAYLOAD_TYPE_ANON_REQ
#undef PAYLOAD_TYPE_PATH
#undef PAYLOAD_TYPE_TRACE
#undef PAYLOAD_TYPE_MULTIPART
#undef PAYLOAD_TYPE_CONTROL
#undef PAYLOAD_TYPE_RAW_CUSTOM
#undef PAYLOAD_VER_1
#undef PAYLOAD_VER_2
#undef PAYLOAD_VER_3
#undef PAYLOAD_VER_4
#undef ADV_TYPE_NONE
#undef ADV_TYPE_CHAT
#undef ADV_TYPE_REPEATER
#undef ADV_TYPE_ROOM
#undef ADV_TYPE_SENSOR
#undef ADV_LATLON_MASK
#undef ADV_FEAT1_MASK
#undef ADV_FEAT2_MASK
#undef ADV_NAME_MASK
#undef TXT_TYPE_PLAIN
#undef TXT_TYPE_CLI_DATA
#undef TXT_TYPE_SIGNED_PLAIN
#undef DATA_TYPE_RESERVED
#undef DATA_TYPE_DEV

extern "C" {
#include "meshcore/types.h"
#include "meshcore_identity.h"
#include "meshcore_packet.h"
#include "meshcore_advert_data.h"
#include "meshcore_txt_data.h"
}

static void require_true(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

static void require_eq_u32(uint32_t actual, uint32_t expected,
                           const char *message) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s: got %u expected %u\n", message, actual,
                 expected);
    std::exit(1);
  }
}

static void require_bytes(const uint8_t *actual, const uint8_t *expected,
                          size_t len, const char *message) {
  if (std::memcmp(actual, expected, len) != 0) {
    std::fprintf(stderr, "FAIL: %s byte mismatch\n", message);
    std::exit(1);
  }
}

static void compare_packet_path_lengths(void) {
  for (unsigned i = 0; i <= 255; ++i) {
    bool upstream = mesh::Packet::isValidPathLen(static_cast<uint8_t>(i));
    bool c_impl = meshcore_packet_is_valid_path_len(static_cast<uint8_t>(i));
    require_true(upstream == c_impl, "packet path length validity parity");
  }
}

static void compare_packet_write_read(void) {
  const uint8_t path[] = {0x10U, 0x20U, 0x30U, 0x40U};
  const uint8_t payload[] = {0xA0U, 0xB1U, 0xC2U};
  uint8_t upstream_raw[MESHCORE_MAX_TRANS_UNIT_LEN] = {};
  uint8_t c_raw[MESHCORE_MAX_TRANS_UNIT_LEN] = {};

  mesh::Packet upstream;
  upstream.header = static_cast<uint8_t>(
      (UP_PAYLOAD_TYPE_TXT_MSG << UP_PH_TYPE_SHIFT) |
      UP_ROUTE_TYPE_TRANSPORT_DIRECT);
  upstream.transport_codes[0] = 0x1234U;
  upstream.transport_codes[1] = 0xABCDU;
  upstream.setPathHashSizeAndCount(2U, 2U);
  std::memcpy(upstream.path, path, sizeof(path));
  upstream.payload_len = sizeof(payload);
  std::memcpy(upstream.payload, payload, sizeof(payload));

  struct meshcore_packet c_packet;
  meshcore_packet_init(&c_packet);
  c_packet.header = upstream.header;
  c_packet.transport_codes[0] = upstream.transport_codes[0];
  c_packet.transport_codes[1] = upstream.transport_codes[1];
  meshcore_packet_set_path_hash_size_and_count(&c_packet, 2U, 2U);
  std::memcpy(c_packet.path, path, sizeof(path));
  c_packet.payload_len = sizeof(payload);
  std::memcpy(c_packet.payload, payload, sizeof(payload));

  uint8_t upstream_len = upstream.writeTo(upstream_raw);
  uint8_t c_len = meshcore_packet_write_to(&c_packet, c_raw);
  require_eq_u32(c_len, upstream_len, "packet write length parity");
  require_bytes(c_raw, upstream_raw, c_len, "packet write bytes parity");

  mesh::Packet upstream_parsed;
  struct meshcore_packet c_parsed;
  meshcore_packet_init(&c_parsed);
  require_true(upstream_parsed.readFrom(upstream_raw, upstream_len),
               "upstream packet read");
  require_true(meshcore_packet_read_from(&c_parsed, upstream_raw, upstream_len),
               "C packet read");
  require_eq_u32(c_parsed.header, upstream_parsed.header,
                 "packet parsed header parity");
  require_eq_u32(c_parsed.path_len, upstream_parsed.path_len,
                 "packet parsed path length parity");
  require_eq_u32(c_parsed.payload_len, upstream_parsed.payload_len,
                 "packet parsed payload length parity");
  require_bytes(c_parsed.path, upstream_parsed.path,
                upstream_parsed.getPathByteLen(), "packet parsed path parity");
  require_bytes(c_parsed.payload, upstream_parsed.payload,
                upstream_parsed.payload_len, "packet parsed payload parity");
}

static void compare_advert_build_parse(void) {
  uint8_t upstream_data[MESHCORE_MAX_ADVERT_DATA_LEN] = {};
  uint8_t c_data[MESHCORE_MAX_ADVERT_DATA_LEN] = {};
  AdvertDataBuilder upstream_builder(UP_ADV_TYPE_CHAT, "node", 1.234567,
                                     -2.5);
  struct meshcore_advert_data_builder c_builder;

  upstream_builder.setFeat1(0x1234U);
  upstream_builder.setFeat2(0xABCDU);
  uint8_t upstream_len = upstream_builder.encodeTo(upstream_data);

  meshcore_advert_data_builder_init_with_name_lat_lon(&c_builder, ADV_TYPE_CHAT,
                                                       "node", 1.234567, -2.5);
  meshcore_advert_data_builder_set_feat1(&c_builder, 0x1234U);
  meshcore_advert_data_builder_set_feat2(&c_builder, 0xABCDU);
  uint8_t c_len = meshcore_advert_data_builder_encode_to(&c_builder, c_data);
  require_eq_u32(c_len, upstream_len, "advert encoded length parity");
  require_bytes(c_data, upstream_data, c_len, "advert encoded bytes parity");

  AdvertDataParser upstream_parser(upstream_data, upstream_len);
  struct meshcore_advert_data_parser c_parser;
  meshcore_advert_data_parser_init(&c_parser, upstream_data, upstream_len);
  require_true(upstream_parser.isValid(), "upstream advert parser valid");
  require_true(meshcore_advert_data_parser_is_valid(&c_parser),
               "C advert parser valid");
  require_eq_u32(meshcore_advert_data_parser_get_type(&c_parser),
                 upstream_parser.getType(), "advert type parity");
  require_eq_u32(meshcore_advert_data_parser_get_feat1(&c_parser),
                 upstream_parser.getFeat1(), "advert feat1 parity");
  require_eq_u32(meshcore_advert_data_parser_get_feat2(&c_parser),
                 upstream_parser.getFeat2(), "advert feat2 parity");
  require_eq_u32(static_cast<uint32_t>(meshcore_advert_data_parser_get_int_lat(&c_parser)),
                 static_cast<uint32_t>(upstream_parser.getIntLat()),
                 "advert latitude parity");
  require_eq_u32(static_cast<uint32_t>(meshcore_advert_data_parser_get_int_lon(&c_parser)),
                 static_cast<uint32_t>(upstream_parser.getIntLon()),
                 "advert longitude parity");
  require_true(std::strcmp(meshcore_advert_data_parser_get_name(&c_parser),
                           upstream_parser.getName()) == 0,
               "advert name parity");
}

static void compare_identity_helpers(void) {
  uint8_t pub_key[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t upstream_hash[4] = {};
  uint8_t c_hash[4] = {};
  for (size_t i = 0; i < sizeof(pub_key); ++i) {
    pub_key[i] = static_cast<uint8_t>(0x30U + i);
  }

  mesh::Identity upstream(pub_key);
  struct meshcore_identity c_identity;
  struct meshcore_identity c_other;
  meshcore_identity_init_from_pub_key(&c_identity, pub_key);

  require_eq_u32(meshcore_identity_copy_hash_to(&c_identity, c_hash),
                 upstream.copyHashTo(upstream_hash),
                 "identity default hash length parity");
  require_bytes(c_hash, upstream_hash, upstream.copyHashTo(upstream_hash),
                "identity default hash bytes parity");

  require_eq_u32(meshcore_identity_copy_hash_by_len(&c_identity, c_hash,
                                                    sizeof(c_hash)),
                 upstream.copyHashTo(upstream_hash, sizeof(upstream_hash)),
                 "identity explicit hash length parity");
  require_bytes(c_hash, upstream_hash, sizeof(c_hash),
                "identity explicit hash bytes parity");
  require_true(meshcore_identity_is_hash_match(&c_identity, upstream_hash) ==
                   upstream.isHashMatch(upstream_hash),
               "identity default hash match parity");
  require_true(meshcore_identity_is_hash_match_by_len(&c_identity, upstream_hash,
                                                      sizeof(upstream_hash)) ==
                   upstream.isHashMatch(upstream_hash, sizeof(upstream_hash)),
               "identity explicit hash match parity");

  meshcore_identity_init_from_pub_key(&c_other, pub_key);
  require_true(meshcore_identity_matches(&c_identity, &c_other),
               "identity equality parity");
  pub_key[0] ^= 0x01U;
  mesh::Identity upstream_other(pub_key);
  meshcore_identity_init_from_pub_key(&c_other, pub_key);
  require_true(meshcore_identity_matches(&c_identity, &c_other) ==
                   upstream.matches(upstream_other),
               "identity mismatch parity");
}

static void compare_txt_helpers(void) {
  char upstream[8];
  char c_impl[8];
  char c_float[16];

  std::memset(upstream, 'x', sizeof(upstream));
  std::memset(c_impl, 'x', sizeof(c_impl));
  StrHelper::strncpy(upstream, "abcdefghi", sizeof(upstream));
  meshcore_txt_strncpy(c_impl, "abcdefghi", sizeof(c_impl));
  require_bytes(reinterpret_cast<uint8_t *>(c_impl),
                reinterpret_cast<uint8_t *>(upstream), sizeof(c_impl),
                "TXT strncpy parity");

  std::memset(upstream, 'x', sizeof(upstream));
  std::memset(c_impl, 'x', sizeof(c_impl));
  StrHelper::strzcpy(upstream, "ab", sizeof(upstream));
  meshcore_txt_strzcpy(c_impl, "ab", sizeof(c_impl));
  require_bytes(reinterpret_cast<uint8_t *>(c_impl),
                reinterpret_cast<uint8_t *>(upstream), sizeof(c_impl),
                "TXT strzcpy parity");

  require_true(meshcore_txt_is_blank("") == StrHelper::isBlank(""),
               "TXT blank empty parity");
  require_true(meshcore_txt_is_blank("   ") == StrHelper::isBlank("   "),
               "TXT blank spaces parity");
  require_true(meshcore_txt_is_blank("  x") == StrHelper::isBlank("  x"),
               "TXT nonblank parity");
  require_eq_u32(meshcore_txt_from_hex("1a2B"), StrHelper::fromHex("1a2B"),
                 "TXT hex mixed-case parity");
  require_eq_u32(meshcore_txt_from_hex("12zz34"), StrHelper::fromHex("12zz34"),
                 "TXT hex stop parity");
  require_eq_u32(meshcore_txt_from_hex("not-hex"), StrHelper::fromHex("not-hex"),
                 "TXT hex invalid parity");

  const float values[] = {0.0f, 1.230f, -1.234f, 12.9996f, -0.0004f};
  for (float value : values) {
    const char *upstream_float = StrHelper::ftoa3(value);
    const char *c_float_result = meshcore_txt_ftoa3(value, c_float,
                                                   sizeof(c_float));
    require_true(std::strcmp(c_float_result, upstream_float) == 0,
                 "TXT ftoa3 parity");
  }
}

static void compare_constants(void) {
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_REQ, UP_PAYLOAD_TYPE_REQ,
                 "PAYLOAD_TYPE_REQ parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_RESPONSE,
                 UP_PAYLOAD_TYPE_RESPONSE,
                 "PAYLOAD_TYPE_RESPONSE parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_TXT_MSG,
                 UP_PAYLOAD_TYPE_TXT_MSG,
                 "PAYLOAD_TYPE_TXT_MSG parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_ACK, UP_PAYLOAD_TYPE_ACK,
                 "PAYLOAD_TYPE_ACK parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_ADVERT, UP_PAYLOAD_TYPE_ADVERT,
                 "PAYLOAD_TYPE_ADVERT parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_GRP_TXT,
                 UP_PAYLOAD_TYPE_GRP_TXT,
                 "PAYLOAD_TYPE_GRP_TXT parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_GRP_DATA,
                 UP_PAYLOAD_TYPE_GRP_DATA,
                 "PAYLOAD_TYPE_GRP_DATA parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_ANON_REQ,
                 UP_PAYLOAD_TYPE_ANON_REQ,
                 "PAYLOAD_TYPE_ANON_REQ parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_PATH, UP_PAYLOAD_TYPE_PATH,
                 "PAYLOAD_TYPE_PATH parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_TRACE, UP_PAYLOAD_TYPE_TRACE,
                 "PAYLOAD_TYPE_TRACE parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_MULTIPART,
                 UP_PAYLOAD_TYPE_MULTIPART,
                 "PAYLOAD_TYPE_MULTIPART parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_CONTROL, UP_PAYLOAD_TYPE_CONTROL,
                 "PAYLOAD_TYPE_CONTROL parity");
  require_eq_u32(MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM,
                 UP_PAYLOAD_TYPE_RAW_CUSTOM, "PAYLOAD_TYPE_RAW_CUSTOM parity");
  require_eq_u32(MESHCORE_TXT_TYPE_PLAIN, UP_TXT_TYPE_PLAIN,
                 "TXT_TYPE_PLAIN parity");
  require_eq_u32(MESHCORE_TXT_TYPE_CLI_DATA, UP_TXT_TYPE_CLI_DATA,
                 "TXT_TYPE_CLI_DATA parity");
  require_eq_u32(MESHCORE_TXT_TYPE_SIGNED_PLAIN, UP_TXT_TYPE_SIGNED_PLAIN,
                 "TXT_TYPE_SIGNED_PLAIN parity");
}

int main(void) {
  compare_constants();
  compare_packet_path_lengths();
  compare_packet_write_read();
  compare_advert_build_parse();
  compare_identity_helpers();
  compare_txt_helpers();
  std::puts("upstream oracle parity checks passed");
  return 0;
}
'''


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--repo-root", default=".", help="repository root")
  parser.add_argument("--meshcore-lib", required=True, help="built meshcore library")
  parser.add_argument("--work-dir", required=True, help="scratch build directory")
  parser.add_argument("--cc", default=None, help="C compiler to use for test support")
  parser.add_argument("--cxx", default=None, help="C++ compiler to use")
  parser.add_argument(
      "--require-reference",
      action="store_true",
      help="fail instead of skipping when .reference/meshcore is absent",
  )
  return parser.parse_args()


def write_stubs(stub_dir: Path) -> None:
  stub_dir.mkdir(parents=True, exist_ok=True)
  (stub_dir / "Arduino.h").write_text(
      textwrap.dedent(
          r'''
          #pragma once
          #include <cstdio>
          #include <cstdlib>
          #include <cstring>

          static inline char *ltoa(long value, char *str, int base) {
            if (base == 10) {
              std::snprintf(str, 32, "%ld", value);
            } else {
              std::snprintf(str, 32, "%lx", value);
            }
            return str;
          }
          '''
      ).lstrip(),
      encoding="utf-8",
  )
  (stub_dir / "SHA256.h").write_text(
      textwrap.dedent(
          r'''
          #pragma once
          #include <cstddef>
          #include <cstdint>
          #include <cstring>

          class SHA256 {
          public:
            void update(const void *, size_t) {}
            void finalize(uint8_t *dest, size_t len) { std::memset(dest, 0, len); }
          };
          '''
      ).lstrip(),
      encoding="utf-8",
  )
  (stub_dir / "Stream.h").write_text(
      textwrap.dedent(
          r'''
          #pragma once
          #include <cstddef>
          #include <cstdint>

          class Stream {
          public:
            virtual ~Stream() {}
            virtual int read() { return -1; }
            virtual size_t write(uint8_t) { return 1; }
            void print(const char *) {}
            void print(char) {}
          };
          '''
      ).lstrip(),
      encoding="utf-8",
  )


def link_flags_from_cmake_cache(build_dir: Path) -> list[str]:
  cache = build_dir / "CMakeCache.txt"
  if not cache.exists():
    return []
  flags: list[str] = []
  text = cache.read_text(encoding="utf-8", errors="ignore")
  for line in text.splitlines():
    if not line.startswith(("CMAKE_C_FLAGS", "CMAKE_EXE_LINKER_FLAGS")):
      continue
    if "=" not in line:
      continue
    for token in line.split("=", 1)[1].split():
      if token.startswith("-fsanitize=") and token not in flags:
        flags.append(token)
  return flags


def main() -> int:
  args = parse_args()
  repo_root = Path(args.repo_root).resolve()
  ref_root = repo_root / ".reference" / "meshcore"
  ref_src = ref_root / "src"
  if not (ref_src / "Packet.cpp").exists():
    message = "locked upstream reference not found; skipping upstream oracle"
    if args.require_reference:
      print(message, file=os.sys.stderr)
      return 1
    print(message)
    return 0

  cxx = args.cxx or os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
  cc = args.cc or os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
  if not cxx:
    print("C++ compiler not found; skipping upstream oracle")
    return 0 if not args.require_reference else 1
  if not cc:
    print("C compiler not found; skipping upstream oracle")
    return 0 if not args.require_reference else 1

  meshcore_lib = Path(args.meshcore_lib).resolve()
  if not meshcore_lib.exists():
    print(f"meshcore library not found: {meshcore_lib}", file=os.sys.stderr)
    return 1

  work_dir = Path(args.work_dir).resolve()
  stub_dir = work_dir / "stubs"
  work_dir.mkdir(parents=True, exist_ok=True)
  write_stubs(stub_dir)
  oracle_cpp = work_dir / "upstream_oracle.cpp"
  oracle_cpp.write_text(ORACLE_SOURCE, encoding="utf-8")
  fake_platform_obj = work_dir / "fake_platform.o"
  exe = work_dir / "upstream_oracle"
  coverage_link_flags = []
  if any(meshcore_lib.parent.rglob("*.gcno")):
    coverage_link_flags.append("--coverage")
  sanitizer_link_flags = link_flags_from_cmake_cache(meshcore_lib.parent)

  cc_cmd = [
      cc,
      "-std=c99",
      "-I",
      str(repo_root / "include"),
      "-I",
      str(repo_root / "src" / "core"),
      "-I",
      str(repo_root / "src" / "support"),
      "-I",
      str(repo_root / "src" / "runtime"),
      "-I",
      str(repo_root / "tests" / "support"),
      "-c",
      str(repo_root / "tests" / "support" / "fake_platform.c"),
      "-o",
      str(fake_platform_obj),
  ]
  subprocess.run(cc_cmd, check=True)

  cxx_cmd = [
      cxx,
      "-std=c++11",
      "-DARDUINO=0",
      "-DMESH_DEBUG=0",
      "-DBRIDGE_DEBUG=0",
      "-include",
      "cstring",
      "-include",
      "cstdio",
      "-include",
      "cstdlib",
      "-I",
      str(stub_dir),
      "-I",
      str(ref_src),
      "-I",
      str(repo_root / "include"),
      "-I",
      str(repo_root / "src" / "core"),
      "-I",
      str(repo_root / "src" / "support"),
      str(oracle_cpp),
      str(ref_src / "Packet.cpp"),
      str(ref_src / "helpers" / "AdvertDataHelpers.cpp"),
      str(ref_src / "helpers" / "TxtDataHelpers.cpp"),
      str(fake_platform_obj),
      str(meshcore_lib),
      "-lm",
      *coverage_link_flags,
      *sanitizer_link_flags,
      "-o",
      str(exe),
  ]
  subprocess.run(cxx_cmd, check=True)
  subprocess.run([str(exe)], check=True)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
