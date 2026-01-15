

#include "platform_sev.h"
#include "../../Common/mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

void platform_sha256(const uint8_t* data, size_t len, uint8_t* hash) {
    if (data == NULL || hash == NULL) return;
    
    
    
    
    
    
    
    memset(hash, 0, 32);
    
    
    for (size_t i = 0; i < len && i < 32; i++) {
        hash[i] = data[i] ^ 0xAA;
    }
}

void* platform_malloc(size_t size) {
    
    
    
    
    
    
    
    return malloc(size);
}

void platform_free(void* ptr) {
    if (ptr == NULL) return;
    
    
    
    
    
    free(ptr);
}

int platform_get_random(uint8_t* buffer, size_t len) {
    if (buffer == NULL) return -1;
    
    
    
    
    
    
    for (size_t i = 0; i < len; i++) {
        buffer[i] = rand() % 256;
    }
    return 0;
}

int platform_encrypt_memory(void* data, size_t len) {
    
    
    return 0;
}

int platform_decrypt_memory(void* data, size_t len) {
    
    return 0;
}

int platform_verify_page(void* page_addr) {
    
    
    return 0;
}

int platform_get_certificate(uint8_t* cert, size_t* cert_len) {
    
    
    if (cert == NULL || cert_len == NULL) return -1;
    memset(cert, 0, *cert_len);
    return 0;
}
