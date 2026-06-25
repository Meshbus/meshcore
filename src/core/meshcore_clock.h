// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_CLOCK_H_
#define MESHCORE_CORE_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal clock boundaries mapped from upstream MillisecondClock / RTCClock.
 */

struct meshcore_rtc_clock_state {
  uint32_t last_unique;
};

unsigned long meshcore_clock_millis_get(void);
void meshcore_clock_millis_override_set(unsigned long value);
void meshcore_clock_millis_override_clear(void);
bool meshcore_clock_millis_override_is_enabled(void);
unsigned long meshcore_clock_millis_override_value_get(void);

uint32_t meshcore_clock_rtc_get_current_time(void);

void meshcore_rtc_clock_state_init(struct meshcore_rtc_clock_state *state);

uint32_t meshcore_clock_rtc_get_current_time_unique(struct meshcore_rtc_clock_state *state);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_CLOCK_H_ */
