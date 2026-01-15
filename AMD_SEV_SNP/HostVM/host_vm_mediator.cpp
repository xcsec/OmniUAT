

#include "host_vm_mediator.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_GUEST_VMS 10

static struct {
    guest_vm_info_t vms[MAX_GUEST_VMS];
    size_t vm_count;
    bool initialized;
    pthread_mutex_t mutex;
} g_mediator_state = {0};

int host_vm_mediator_init(void) {
    if (g_mediator_state.initialized) {
        return 0;
    }
    
    memset(&g_mediator_state, 0, sizeof(g_mediator_state));
    pthread_mutex_init(&g_mediator_state.mutex, NULL);
    
    g_mediator_state.initialized = true;

    return 0;
}

int host_vm_mediator_register_vm(uint32_t vm_id, void* vm_handle) {
    if (!g_mediator_state.initialized) return -1;
    if (g_mediator_state.vm_count >= MAX_GUEST_VMS) return -1;
    
    pthread_mutex_lock(&g_mediator_state.mutex);
    
    
    for (size_t i = 0; i < g_mediator_state.vm_count; i++) {
        if (g_mediator_state.vms[i].vm_id == vm_id) {
            pthread_mutex_unlock(&g_mediator_state.mutex);

            return 0;
        }
    }
    
    
    guest_vm_info_t* vm = &g_mediator_state.vms[g_mediator_state.vm_count];
    vm->vm_id = vm_id;
    vm->vm_handle = vm_handle;
    vm->is_active = true;
    memset(vm->vm_cert, 0, 2048);
    
    g_mediator_state.vm_count++;
    
    pthread_mutex_unlock(&g_mediator_state.mutex);
    
           vm_id, g_mediator_state.vm_count);
    
    return 0;
}

static guest_vm_info_t* find_vm(uint32_t vm_id) {
    for (size_t i = 0; i < g_mediator_state.vm_count; i++) {
        if (g_mediator_state.vms[i].vm_id == vm_id &&
            g_mediator_state.vms[i].is_active) {
            return &g_mediator_state.vms[i];
        }
    }
    return NULL;
}

int host_vm_mediator_forward_message(uint32_t src_vm_id,
                                      uint32_t dst_vm_id,
                                      const uint8_t* data,
                                      size_t len) {
    if (!g_mediator_state.initialized) return -1;
    if (data == NULL || len == 0) return -1;
    
    pthread_mutex_lock(&g_mediator_state.mutex);
    
    
    guest_vm_info_t* dst_vm = find_vm(dst_vm_id);
    if (dst_vm == NULL) {
        pthread_mutex_unlock(&g_mediator_state.mutex);

        return -1;
    }
    
    
    
    
    
    
           src_vm_id, dst_vm_id, len);
    
    pthread_mutex_unlock(&g_mediator_state.mutex);
    
    return 0;
}

int host_vm_mediator_broadcast(uint32_t src_vm_id,
                                const uint8_t* data,
                                size_t len) {
    if (!g_mediator_state.initialized) return -1;
    
    pthread_mutex_lock(&g_mediator_state.mutex);
    
    int success_count = 0;
    for (size_t i = 0; i < g_mediator_state.vm_count; i++) {
        if (g_mediator_state.vms[i].vm_id != src_vm_id &&
            g_mediator_state.vms[i].is_active) {
            if (host_vm_mediator_forward_message(src_vm_id,
                                                  g_mediator_state.vms[i].vm_id,
                                                  data, len) == 0) {
                success_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&g_mediator_state.mutex);

    return (success_count > 0) ? 0 : -1;
}

int host_vm_mediator_receive_from_vm(uint32_t vm_id,
                                       uint8_t* buffer,
                                       size_t* buffer_len) {
    if (!g_mediator_state.initialized) return -1;
    
    
    
    
    
    
    *buffer_len = 0;
    return -1;
}

void host_vm_mediator_close(void) {
    if (!g_mediator_state.initialized) return;
    
    pthread_mutex_lock(&g_mediator_state.mutex);
    
    
    for (size_t i = 0; i < g_mediator_state.vm_count; i++) {
        g_mediator_state.vms[i].is_active = false;
    }
    g_mediator_state.vm_count = 0;
    
    pthread_mutex_unlock(&g_mediator_state.mutex);
    pthread_mutex_destroy(&g_mediator_state.mutex);
    
    g_mediator_state.initialized = false;

}
