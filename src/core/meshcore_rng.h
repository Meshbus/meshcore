// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_RNG_H_
#define MESHCORE_CORE_RNG_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal RNG boundary mapped from upstream Utils.h RNG.
 */

void meshcore_rng_random(uint8_t *dest, size_t size);
uint32_t meshcore_rng_next_int(uint32_t min, uint32_t max);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_RNG_H_ */
