// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_TESTS_NATIVE_NATIVE_TEST_H_
#define MESHCORE_TESTS_NATIVE_NATIVE_TEST_H_

#include <stdio.h>

#define NATIVE_TEST_ASSERT(cond)                                               \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__,    \
              #cond);                                                          \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define NATIVE_TEST_ASSERT_EQ(expected, actual)                                \
  do {                                                                         \
    unsigned long long native_expected_ = (unsigned long long)(expected);       \
    unsigned long long native_actual_ = (unsigned long long)(actual);           \
    if (native_expected_ != native_actual_) {                                   \
      fprintf(stderr, "%s:%d: expected %llu, got %llu\n", __FILE__, __LINE__, \
              native_expected_, native_actual_);                               \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#endif /* MESHCORE_TESTS_NATIVE_NATIVE_TEST_H_ */
