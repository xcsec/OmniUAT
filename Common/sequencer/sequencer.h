#ifndef _SEQUENCER_H_
#define _SEQUENCER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "mpt_tree.h"

#define MAX_LOG_ENTRIES 10000
#define MAX_TOKEN_ADDRESS_LEN 42  
#define MAX_NODES 10

typedef enum {
    LOG_TRANSFER = 0,
    LOG_APPROVE = 1,
    LOG_MINT = 2,
    LOG_BURN = 3
} log_type_t;

typedef struct {
    uint64_t sequence_id;      
    uint64_t timestamp;        
    log_type_t type;           
    uint8_t token_address[MAX_TOKEN_ADDRESS_LEN];  
    uint8_t from[20];          
    uint8_t to[20];            
    uint8_t amount[32];         
    uint8_t signature[65];     
    bool processed;            
} log_entry_t;

typedef struct {
    uint32_t node_id;
    uint8_t public_key[64];    
    bool is_active;
} sequencer_node_t;

typedef struct {
    mpt_tree_t token_trees[MAX_NODES];  
    uint8_t token_addresses[MAX_NODES][MAX_TOKEN_ADDRESS_LEN];
    size_t token_count;
    
    log_entry_t log_queue[MAX_LOG_ENTRIES];
    size_t log_count;
    uint64_t next_sequence_id;
    
    sequencer_node_t nodes[MAX_NODES];
    size_t node_count;
    uint32_t current_leader;   
} sequencer_state_t;

int sequencer_init(sequencer_state_t* state);

int sequencer_add_log(sequencer_state_t* state, const log_entry_t* log);

int sequencer_process_logs(sequencer_state_t* state);

int sequencer_get_token_root(sequencer_state_t* state, 
                              const uint8_t* token_address,
                              uint8_t* root_hash);

int sequencer_get_balance(sequencer_state_t* state,
                           const uint8_t* token_address,
                           const uint8_t* account,
                           uint8_t* balance);

bool sequencer_verify_log_signature(const log_entry_t* log);

#endif
