// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_
#define MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_

#include <stdint.h>

#include "meshcore/types.h"

void meshcore_native_platform_reset(void);
unsigned int meshcore_native_platform_timer_arm_count_get(void);
uint32_t meshcore_native_platform_last_timer_deadline_get(void);
unsigned int meshcore_native_platform_message_count_get(void);
const meshcore_common_message_t *meshcore_native_platform_last_message_get(void);

#endif /* MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_ */
