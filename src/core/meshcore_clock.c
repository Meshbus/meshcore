// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_clock.h"

#include <stdbool.h>
#include <string.h>

#include "meshcore_platform_bridge.h"

static struct {
  bool enabled;
  unsigned long value;
} s_millis_override;

unsigned long meshcore_clock_millis_get(void) {
  if (s_millis_override.enabled) {
    return s_millis_override.value;
  }

  return meshcore_platform_bridge_millis_get();
}

void meshcore_clock_millis_override_set(unsigned long value) {
  s_millis_override.enabled = true;
  s_millis_override.value = value;
}

void meshcore_clock_millis_override_clear(void) {
  memset(&s_millis_override, 0, sizeof(s_millis_override));
}

bool meshcore_clock_millis_override_is_enabled(void) {
  return s_millis_override.enabled;
}

unsigned long meshcore_clock_millis_override_value_get(void) {
  return s_millis_override.value;
}

uint32_t meshcore_clock_rtc_get_current_time(void) {
  return meshcore_platform_bridge_rtc_get_current_time();
}

void meshcore_rtc_clock_state_init(struct meshcore_rtc_clock_state *state) {
  if (state == NULL) {
    return;
  }

  memset(state, 0, sizeof(*state));
}

uint32_t meshcore_clock_rtc_get_current_time_unique(
    struct meshcore_rtc_clock_state *state) {
  uint32_t now = meshcore_clock_rtc_get_current_time();

  if (state == NULL) {
    return now;
  }

  if (now <= state->last_unique) {
    state->last_unique++;
    return state->last_unique;
  }

  state->last_unique = now;
  return now;
}
