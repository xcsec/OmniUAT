#ifndef _SEV_VM_COMMUNICATION_H_
#define _SEV_VM_COMMUNICATION_H_

#include <stdint.h>
#include <stddef.h>

typedef enum {
    SEV_COMM_CHANNEL_HOST_MEDIATED = 0,  
    SEV_COMM_CHANNEL_SHARED_MEMORY = 1,  
    SEV_COMM_CHANNEL_VIRTUAL_NET = 2,    
    SEV_COMM_CHANNEL_SECURE_CHANNEL = 3  
} sev_comm_channel_type_t;

typedef struct {
    uint32_t src_vm_id;      
    uint32_t dst_vm_id;      
    uint32_t msg_type;      
    uint64_t sequence;      
    uint8_t signature[64];  
    size_t payload_len;      
    uint8_t payload[];      
} sev_vm_message_t;

int sev_vm_comm_init(uint32_t vm_id, sev_comm_channel_type_t channel_type);

int sev_vm_comm_send(uint32_t dst_vm_id, const uint8_t* data, size_t len);

int sev_vm_comm_receive(uint8_t* buffer, size_t* buffer_len, uint32_t* src_vm_id);

int sev_vm_comm_broadcast(const uint8_t* data, size_t len);

typedef void (*sev_vm_message_callback_t)(uint32_t src_vm_id, 
                                           const uint8_t* data, 
                                           size_t len);
int sev_vm_comm_register_callback(sev_vm_message_callback_t callback);

void sev_vm_comm_close(void);

#endif
