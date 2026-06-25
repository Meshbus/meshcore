// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include <stdio.h>

#include <meshcore/runtime.h>

int main(void)
{
  int rc = meshcore_init();

  if (rc != 0) {
    fprintf(stderr, "meshcore_init failed: %d\n", rc);
    return 1;
  }

  meshcore_deinit();
  puts("meshcore minimal host initialized");
  return 0;
}
