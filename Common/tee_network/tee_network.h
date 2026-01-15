#ifndef _TEE_NETWORK_H_
#define _TEE_NETWORK_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_NODES 10
#define MAX_MESSAGE_SIZE 4096
#define MAX_PENDING_MESSAGES 100

typedef enum {
    MSG_HEARTBEAT = 0,
    MSG_LEADER_ELECTION = 1,
    MSG_SORTED_TXS = 2,
    MSG_DAG_NODE = 3,
    MSG_REQUEST_DAG_NODE = 4,
    MSG_DAG_NODE_RESPONSE = 5,
    MSG_SYNC_REQUEST = 6,
    MSG_SYNC_RESPONSE = 7,
    MSG_RAFT_REQUEST_VOTE = 8,
    MSG_RAFT_REQUEST_VOTE_RESPONSE = 9,
    MSG_RAFT_APPEND_ENTRIES = 10,
    MSG_RAFT_APPEND_ENTRIES_RESPONSE = 11,
    MSG_TX_SET_BROADCAST = 12,        
    MSG_TX_SET_SIGNATURE = 13,        
    MSG_EPOCH_OUTPUT = 14,            
    MSG_EPOCH_SYNC_TO_L2 = 15         
} message_type_t;

typedef struct {
    uint32_t from_node_id;
    uint32_t to_node_id;
    message_type_t type;
    uint32_t payload_size;
    uint64_t timestamp;
    uint8_t signature[64];  
} message_header_t;

typedef struct {
    message_header_t header;
    uint8_t payload[MAX_MESSAGE_SIZE];
} tee_message_t;

typedef struct {
    uint32_t node_id;
    char ip_address[64];     
    uint16_t port;
    uint8_t public_key[64];  
    bool is_active;
    uint64_t last_seen;
} node_address_t;

typedef struct {
    node_address_t nodes[MAX_NODES];
    size_t node_count;
    uint32_t my_node_id;
    
    
    tee_message_t pending_messages[MAX_PENDING_MESSAGES];
    size_t pending_count;
    
    
    void* socket_handle;  
} tee_network_state_t;

int tee_network_init(tee_network_state_t* network, uint32_t node_id, 
                      const char* listen_ip, uint16_t listen_port);

int tee_network_add_node(tee_network_state_t* network,
                          uint32_t node_id,
                          const char* ip_address,
                          uint16_t port,
                          const uint8_t* public_key);

int tee_network_send_message(tee_network_state_t* network,
                               uint32_t to_node_id,
                               message_type_t type,
                               const uint8_t* payload,
                               size_t payload_size);

int tee_network_receive_message(tee_network_state_t* network,
                                  tee_message_t* message);

int tee_network_broadcast(tee_network_state_t* network,
                           message_type_t type,
                           const uint8_t* payload,
                           size_t payload_size);

bool tee_network_verify_message(const tee_message_t* message);

int tee_network_sign_message(tee_message_t* message);

int tee_network_send_heartbeat(tee_network_state_t* network);

void tee_network_cleanup(tee_network_state_t* network);

#endif
