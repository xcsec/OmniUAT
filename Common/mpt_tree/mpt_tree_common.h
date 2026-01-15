#ifndef _MPT_TREE_COMMON_H_
#define _MPT_TREE_COMMON_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void platform_sha256(const uint8_t* data, size_t len, uint8_t* hash);

void* platform_malloc(size_t size);
void platform_free(void* ptr);

int platform_get_random(uint8_t* buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif
