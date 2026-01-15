#ifndef _PLATFORM_SEV_H_
#define _PLATFORM_SEV_H_

#include <stdint.h>
#include <stddef.h>
#include "../../Common/mpt_tree/mpt_tree_common.h"

int platform_encrypt_memory(void* data, size_t len);
int platform_decrypt_memory(void* data, size_t len);

int platform_verify_page(void* page_addr);

int platform_get_certificate(uint8_t* cert, size_t* cert_len);

#endif
