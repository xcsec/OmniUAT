#include "tee_network.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

int tee_network_init(tee_network_state_t* network, uint32_t node_id,
                      const char* listen_ip, uint16_t listen_port) {
    if (network == NULL || listen_ip == NULL) return -1;
    
    memset(network, 0, sizeof(tee_network_state_t));
    network->my_node_id = node_id;
    
    
    
    
    return 0;
}

int tee_network_add_node(tee_network_state_t* network,
                          uint32_t node_id,
                          const char* ip_address,
                          uint16_t port,
                          const uint8_t* public_key) {
    if (network == NULL || ip_address == NULL || public_key == NULL) {
        return -1;
    }
    
    if (network->node_count >= MAX_NODES) return -1;
    
    
    for (size_t i = 0; i < network->node_count; i++) {
        if (network->nodes[i].node_id == node_id) {
            
            strncpy(network->nodes[i].ip_address, ip_address, 63);
            network->nodes[i].port = port;
            memcpy(network->nodes[i].public_key, public_key, 64);
            network->nodes[i].is_active = true;
            return 0;
        }
    }
    
    
    size_t idx = network->node_count;
    network->nodes[idx].node_id = node_id;
    strncpy(network->nodes[idx].ip_address, ip_address, 63);
    network->nodes[idx].port = port;
    memcpy(network->nodes[idx].public_key, public_key, 64);
    network->nodes[idx].is_active = true;
    network->node_count++;
    
    return 0;
}

int tee_network_sign_message(tee_message_t* message) {
    if (message == NULL) return -1;
    
    
    uint8_t hash[32];
    uint8_t buffer[sizeof(message_header_t) + MAX_MESSAGE_SIZE];
    size_t offset = 0;
    
    memcpy(buffer + offset, &message->header.from_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &message->header.to_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &message->header.type, sizeof(message_type_t));
    offset += sizeof(message_type_t);
    memcpy(buffer + offset, &message->header.payload_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, message->payload, message->header.payload_size);
    offset += message->header.payload_size;
    
    
    platform_sha256(buffer, offset, hash);
    
    
    
    memcpy(message->header.signature, hash, 32);
    memset(message->header.signature + 32, 0, 32);
    
    return 0;
}

bool tee_network_verify_message(const tee_message_t* message) {
    if (message == NULL) return false;
    
    
    uint8_t hash[32];
    uint8_t buffer[sizeof(message_header_t) + MAX_MESSAGE_SIZE];
    size_t offset = 0;
    
    memcpy(buffer + offset, &message->header.from_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &message->header.to_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &message->header.type, sizeof(message_type_t));
    offset += sizeof(message_type_t);
    memcpy(buffer + offset, &message->header.payload_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, message->payload, message->header.payload_size);
    offset += message->header.payload_size;
    
    
    platform_sha256(buffer, offset, hash);
    
    
    
    bool all_zero = true;
    for (int i = 0; i < 64; i++) {
        if (message->header.signature[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    return !all_zero;
}

int tee_network_send_message(tee_network_state_t* network,
                               uint32_t to_node_id,
                               message_type_t type,
                               const uint8_t* payload,
                               size_t payload_size) {
    if (network == NULL || payload == NULL) return -1;
    
    if (payload_size > MAX_MESSAGE_SIZE) return -1;
    
    
    node_address_t* target_node = NULL;
    for (size_t i = 0; i < network->node_count; i++) {
        if (network->nodes[i].node_id == to_node_id) {
            target_node = &network->nodes[i];
            break;
        }
    }
    
    if (target_node == NULL || !target_node->is_active) {
        return -1;
    }
    
    
    tee_message_t message;
    memset(&message, 0, sizeof(tee_message_t));
    message.header.from_node_id = network->my_node_id;
    message.header.to_node_id = to_node_id;
    message.header.type = type;
    message.header.payload_size = payload_size;
    message.header.timestamp = 0; 
    memcpy(message.payload, payload, payload_size);
    
    
    if (tee_network_sign_message(&message) != 0) {
        return -1;
    }
    
    
    
    
    return 0;
}

int tee_network_receive_message(tee_network_state_t* network,
                                  tee_message_t* message) {
    if (network == NULL || message == NULL) return -1;
    
    
    if (network->pending_count > 0) {
        memcpy(message, &network->pending_messages[0], sizeof(tee_message_t));
        
        
        for (size_t i = 1; i < network->pending_count; i++) {
            memcpy(&network->pending_messages[i-1], 
                   &network->pending_messages[i], 
                   sizeof(tee_message_t));
        }
        network->pending_count--;
        
        
        if (!tee_network_verify_message(message)) {
            return -1; 
        }
        
        return 0;
    }
    
    return -1; 
}

int tee_network_broadcast(tee_network_state_t* network,
                           message_type_t type,
                           const uint8_t* payload,
                           size_t payload_size) {
    if (network == NULL || payload == NULL) return -1;
    
    int success_count = 0;
    
    
    for (size_t i = 0; i < network->node_count; i++) {
        if (network->nodes[i].is_active && 
            network->nodes[i].node_id != network->my_node_id) {
            if (tee_network_send_message(network, 
                                         network->nodes[i].node_id,
                                         type, payload, payload_size) == 0) {
                success_count++;
            }
        }
    }
    
    return (success_count > 0) ? 0 : -1;
}

int tee_network_send_heartbeat(tee_network_state_t* network) {
    if (network == NULL) return -1;
    
    uint8_t heartbeat_payload[8];
    memset(heartbeat_payload, 0, 8);
    
    return tee_network_broadcast(network, MSG_HEARTBEAT, 
                                  heartbeat_payload, 8);
}

void tee_network_cleanup(tee_network_state_t* network) {
    if (network == NULL) return;
    
    
    network->pending_count = 0;
    
    
    memset(network, 0, sizeof(tee_network_state_t));
}
