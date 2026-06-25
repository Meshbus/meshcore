// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_TXT_DATA_H_
#define MESHCORE_SUPPORT_TXT_DATA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal TXT/group-data compatibility helpers promoted from upstream
 * TxtDataHelpers. Legacy board-framework float formatting is intentionally not
 * part of the generic runtime contract.
 */

#define MESHCORE_TXT_TYPE_PLAIN 0U
#define MESHCORE_TXT_TYPE_CLI_DATA 1U
#define MESHCORE_TXT_TYPE_SIGNED_PLAIN 2U

#define MESHCORE_TXT_DATA_TYPE_RESERVED MESHCORE_CHANNEL_DATA_TYPE_RESERVED
#define MESHCORE_TXT_DATA_TYPE_DEV MESHCORE_CHANNEL_DATA_TYPE_DEV

void meshcore_txt_strncpy(char *dest, const char *src, size_t buf_sz);
void meshcore_txt_strzcpy(char *dest, const char *src, size_t buf_sz);
bool meshcore_txt_is_blank(const char *str);
uint32_t meshcore_txt_from_hex(const char *src);
const char *meshcore_txt_ftoa3(float value, char *dest, size_t dest_len);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_TXT_DATA_H_ */
