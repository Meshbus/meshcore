// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_rng.h"

#include "meshcore_platform_bridge.h"

void meshcore_rng_random(uint8_t *dest, size_t size)
{
  if (dest == NULL || size == 0U) {
    return;
  }

  meshcore_platform_bridge_rng_random(dest, size);
}

uint32_t meshcore_rng_next_int(uint32_t min, uint32_t max)
{
  uint32_t num;

  if (max <= min) {
    return min;
  }

  meshcore_rng_random((uint8_t *)&num, sizeof(num));
  return (num % (max - min)) + min;
}
