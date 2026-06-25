// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_txt_data.h"

#include <stdio.h>

void meshcore_txt_strncpy(char *dest, const char *src, size_t buf_sz)
{
  if (dest == NULL || src == NULL || buf_sz == 0U) {
    return;
  }

  while (buf_sz > 1U && *src != '\0') {
    *dest++ = *src++;
    buf_sz--;
  }
  *dest = '\0';
}

void meshcore_txt_strzcpy(char *dest, const char *src, size_t buf_sz)
{
  if (dest == NULL || src == NULL) {
    return;
  }

  while (buf_sz > 1U && *src != '\0') {
    *dest++ = *src++;
    buf_sz--;
  }
  while (buf_sz > 0U) {
    *dest++ = '\0';
    buf_sz--;
  }
}

bool meshcore_txt_is_blank(const char *str)
{
  if (str == NULL) {
    return true;
  }

  while (*str != '\0') {
    if (*str++ != ' ') {
      return false;
    }
  }
  return true;
}

uint32_t meshcore_txt_from_hex(const char *src)
{
  uint32_t n = 0U;

  if (src == NULL) {
    return 0U;
  }

  while (*src != '\0') {
    if (*src >= '0' && *src <= '9') {
      n <<= 4U;
      n |= (uint32_t)(*src - '0');
    } else if (*src >= 'A' && *src <= 'F') {
      n <<= 4U;
      n |= (uint32_t)(*src - 'A' + 10);
    } else if (*src >= 'a' && *src <= 'f') {
      n <<= 4U;
      n |= (uint32_t)(*src - 'a' + 10);
    } else {
      break;
    }
    src++;
  }

  return n;
}

const char *meshcore_txt_ftoa3(float value, char *dest, size_t dest_len)
{
  int scaled;
  int whole;
  int decimals;
  int len;

  if (dest == NULL || dest_len == 0U) {
    return "";
  }

  scaled = (int)(value * 1000.0f + (value >= 0.0f ? 0.5f : -0.5f));
  whole = scaled / 1000;
  decimals = scaled % 1000;
  if (decimals < 0) {
    decimals = -decimals;
  }

  (void)snprintf(dest, dest_len, "%d.%03d", whole, decimals);
  len = 0;
  while (dest[len] != '\0') {
    len++;
  }
  while (len > 0 && dest[len - 1] == '0') {
    dest[--len] = '\0';
  }
  if (len > 0 && dest[len - 1] == '.') {
    dest[len - 1] = '\0';
  }

  return dest;
}
