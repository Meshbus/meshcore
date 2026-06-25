// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include "meshcore/platform.h"
#include "meshcore/runtime.h"
#include "meshcore/types.h"

int main(void)
{
  meshcore_common_packet_view_t packet = {0};
  meshcore_common_identity_view_t identity = {0};
  meshcore_common_channel_view_t channel = {0};

  NATIVE_TEST_ASSERT_EQ(26U, MESHCORE_ABI_VERSION);
  NATIVE_TEST_ASSERT_EQ(32U, MESHCORE_PUBLIC_KEY_SIZE);
  NATIVE_TEST_ASSERT_EQ(64U, MESHCORE_MAX_PATH_LEN);
  NATIVE_TEST_ASSERT_EQ(0U, packet.payload_type);
  NATIVE_TEST_ASSERT_EQ(0U, identity.public_key[0]);
  NATIVE_TEST_ASSERT_EQ(0U, channel.secret[0]);

  return 0;
}
