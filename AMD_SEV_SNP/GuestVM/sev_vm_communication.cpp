

#include "sev_vm_communication.h"
#include "platform_sev.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static struct {
    uint32_t vm_id;
    sev_comm_channel_type_t channel_type;
    bool initialized;
    sev_vm_message_callback_t callback;
    void* channel_handle;  
} g_comm_state = {0};

static int send_via_host(uint32_t dst_vm_id, const uint8_t* data, size_t len) {
    if (!g_comm_state.initialized) return -1;
    
    
    sev_vm_message_t* msg = (sev_vm_message_t*)platform_malloc(
        sizeof(sev_vm_message_t) + len);
    if (msg == NULL) return -1;
    
    msg->src_vm_id = g_comm_state.vm_id;
    msg->dst_vm_id = dst_vm_id;
    msg->msg_type = 0;  
    msg->sequence = 0;  
    msg->payload_len = len;
    memcpy(msg->payload, data, len);
    
    
    
    memset(msg->signature, 0, 64);
    
    
    
    
    
    
           g_comm_state.vm_id, dst_vm_id, len);
    
    platform_free(msg);
    return 0;
}

static int receive_via_host(uint8_t* buffer, size_t* buffer_len, 
                             uint32_t* src_vm_id) {
    if (!g_comm_state.initialized) return -1;
    
    
    
    
    
    
    *buffer_len = 0;
    return -1;  
}

static int init_shared_memory(void) {

    return 0;
}

static int send_via_shared_mem(uint32_t dst_vm_id, const uint8_t* data, size_t len) {
    if (g_comm_state.channel_handle == NULL) return -1;
    
    
    
    
    
           g_comm_state.vm_id, dst_vm_id);
    return 0;
}

static int init_virtual_network(void) {

    return 0;
}

static int send_via_network(uint32_t dst_vm_id, const uint8_t* data, size_t len) {
    
    
    
    
           g_comm_state.vm_id, dst_vm_id);
    return 0;
}

int sev_vm_comm_init(uint32_t vm_id, sev_comm_channel_type_t channel_type) {
    if (g_comm_state.initialized) {

        return 0;
    }
    
    g_comm_state.vm_id = vm_id;
    g_comm_state.channel_type = channel_type;
    
    int ret = -1;
    switch (channel_type) {
        case SEV_COMM_CHANNEL_HOST_MEDIATED:
            
            ret = 0;
            break;
            
        case SEV_COMM_CHANNEL_SHARED_MEMORY:
            ret = init_shared_memory();
            break;
            
        case SEV_COMM_CHANNEL_VIRTUAL_NET:
            ret = init_virtual_network();
            break;
            
        case SEV_COMM_CHANNEL_SECURE_CHANNEL:
            
            
            ret = 0;
            break;
            
        default:

            return -1;
    }
    
    if (ret == 0) {
        g_comm_state.initialized = true;
               vm_id, channel_type);
    }
    
    return ret;
}

int sev_vm_comm_send(uint32_t dst_vm_id, const uint8_t* data, size_t len) {
    if (!g_comm_state.initialized) return -1;
    if (data == NULL || len == 0) return -1;
    
    switch (g_comm_state.channel_type) {
        case SEV_COMM_CHANNEL_HOST_MEDIATED:
            return send_via_host(dst_vm_id, data, len);
            
        case SEV_COMM_CHANNEL_SHARED_MEMORY:
            return send_via_shared_mem(dst_vm_id, data, len);
            
        case SEV_COMM_CHANNEL_VIRTUAL_NET:
            return send_via_network(dst_vm_id, data, len);
            
        case SEV_COMM_CHANNEL_SECURE_CHANNEL:
            
            return send_via_host(dst_vm_id, data, len);  
            
        default:
            return -1;
    }
}

int sev_vm_comm_receive(uint8_t* buffer, size_t* buffer_len, uint32_t* src_vm_id) {
    if (!g_comm_state.initialized) return -1;
    if (buffer == NULL || buffer_len == NULL || src_vm_id == NULL) return -1;
    
    switch (g_comm_state.channel_type) {
        case SEV_COMM_CHANNEL_HOST_MEDIATED:
            return receive_via_host(buffer, buffer_len, src_vm_id);
            
        case SEV_COMM_CHANNEL_SHARED_MEMORY:
            
            return -1;  
            
        case SEV_COMM_CHANNEL_VIRTUAL_NET:
            
            return -1;  
            
        default:
            return -1;
    }
}

int sev_vm_comm_broadcast(const uint8_t* data, size_t len) {
    if (!g_comm_state.initialized) return -1;
    
    
    
    for (uint32_t i = 1; i <= 10; i++) {
        if (i != g_comm_state.vm_id) {
            sev_vm_comm_send(i, data, len);
        }
    }
    
    return 0;
}

int sev_vm_comm_register_callback(sev_vm_message_callback_t callback) {
    if (callback == NULL) return -1;
    g_comm_state.callback = callback;
    return 0;
}

void sev_vm_comm_close(void) {
    if (!g_comm_state.initialized) return;
    
    
    if (g_comm_state.channel_handle != NULL) {
        platform_free(g_comm_state.channel_handle);
        g_comm_state.channel_handle = NULL;
    }
    
    memset(&g_comm_state, 0, sizeof(g_comm_state));

}
