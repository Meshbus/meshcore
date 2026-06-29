// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"
#include "fake_platform.h"

#include <stdint.h>

#include "meshcore/runtime.h"
#include "meshcore_advert_data.h"
#include "meshcore_packet.h"
#include "meshcore_cayenne_lpp_compat.h"

/*
 * Deterministic fuzz smoke for CI. Long-running fuzzers can reuse these same
 * parser/runtime targets, while ordinary PR CI keeps this bounded and stable.
 */

static uint32_t fuzz_next(uint32_t *state)
{
  *state = (*state * 1664525U) + 1013904223U;
  return *state;
}

static void fuzz_fill(uint32_t *state, uint8_t *buf, size_t len)
{
  size_t i;

  for (i = 0U; i < len; i++) {
    buf[i] = (uint8_t)(fuzz_next(state) >> 24);
  }
}

static int test_packet_parser_fuzz_smoke(void)
{
  uint32_t state = 0xC001D00DU;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t out[MESHCORE_MAX_TRANS_UNIT_LEN];
  struct meshcore_packet packet;
  int i;

  for (i = 0; i < 512; i++) {
    uint8_t len = (uint8_t)(fuzz_next(&state) & 0xFFU);

    fuzz_fill(&state, raw, len);
    meshcore_packet_init(&packet);
    if (meshcore_packet_read_from(&packet, raw, len)) {
      uint8_t written = meshcore_packet_write_to(&packet, out);

      NATIVE_TEST_ASSERT(written > 0U);
    }
  }

  return 0;
}

static int test_advert_parser_fuzz_smoke(void)
{
  uint32_t state = 0xAD7E2715U;
  uint8_t raw[MESHCORE_MAX_ADVERT_DATA_LEN + 8U];
  struct meshcore_advert_data_parser parser;
  int i;

  for (i = 0; i < 512; i++) {
    uint8_t len = (uint8_t)(fuzz_next(&state) % sizeof(raw));

    fuzz_fill(&state, raw, len);
    meshcore_advert_data_parser_init(&parser, raw, len);
    if (meshcore_advert_data_parser_is_valid(&parser)) {
      NATIVE_TEST_ASSERT(meshcore_advert_data_parser_get_type(&parser) <= 0x0FU);
    }
  }

  return 0;
}

static int test_cayenne_lpp_parser_fuzz_smoke(void)
{
  uint32_t state = 0x7E1EAA11U;
  uint8_t raw[96];
  struct cayenne_lpp_location location;
  int i;

  for (i = 0; i < 512; i++) {
    uint8_t len = (uint8_t)(fuzz_next(&state) % sizeof(raw));

    fuzz_fill(&state, raw, len);
    cayenne_lpp_parse_location(raw, len, &location);
    if (location.has_latitude || location.has_longitude) {
      NATIVE_TEST_ASSERT(location.has_latitude);
      NATIVE_TEST_ASSERT(location.has_longitude);
    }
  }

  return 0;
}

static int test_runtime_rx_fuzz_smoke(void)
{
  uint32_t state = 0xF00DCAFEU;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  int i;

  meshcore_native_platform_reset();
  meshcore_deinit();
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());

  for (i = 0; i < 256; i++) {
    size_t len = (size_t)((fuzz_next(&state) % MESHCORE_MAX_TRANS_UNIT_LEN) + 1U);
    int rc;

    fuzz_fill(&state, raw, len);
    rc = meshcore_radio_rx_inject(raw, len, -90, (int8_t)(i & 0x1F), (uint32_t)(i + 1));
    NATIVE_TEST_ASSERT(rc <= 0);
  }

  meshcore_deinit();
  return 0;
}

int main(void)
{
  NATIVE_TEST_ASSERT_EQ(0, test_packet_parser_fuzz_smoke());
  NATIVE_TEST_ASSERT_EQ(0, test_advert_parser_fuzz_smoke());
  NATIVE_TEST_ASSERT_EQ(0, test_cayenne_lpp_parser_fuzz_smoke());
  NATIVE_TEST_ASSERT_EQ(0, test_runtime_rx_fuzz_smoke());

  return 0;
}
