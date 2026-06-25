// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_GROUP_CHANNEL_H_
#define MESHCORE_CORE_GROUP_CHANNEL_H_

#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal group-channel data model mapped from upstream mesh::GroupChannel.
 */
struct meshcore_group_channel {
  uint8_t hash[MESHCORE_CHANNEL_HASH_BYTES];
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_GROUP_CHANNEL_H_ */
