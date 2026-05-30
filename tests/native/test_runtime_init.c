// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"
#include "fake_platform.h"

#include "meshcore/runtime.h"

int main(void)
{
  meshcore_native_platform_reset();

  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  NATIVE_TEST_ASSERT(meshcore_native_platform_timer_arm_count_get() > 0U);
  NATIVE_TEST_ASSERT(meshcore_native_platform_last_timer_deadline_get() > 0U);

  meshcore_deinit();
  return 0;
}
