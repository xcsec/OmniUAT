#ifndef _L2_FULL_NODE_H_
#define _L2_FULL_NODE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_L2_CHAINS 16
#define MAX_BLOCK_HEADERS 10000
#define MAX_LOG_ENTRIES_PER_BLOCK 1000

typedef struct {
    uint64_t block_number;
    uint8_t block_hash[32];
    uint8_t parent_hash[32];
    uint8_t state_root[32];
    uint8_t receipts_root[32];
    uint8_t logs_bloom[256];  
    uint64_t timestamp;
    uint32_t chain_id;
} l2_block_header_t;

typedef struct {
    uint8_t tx_hash[32];
    uint32_t log_index;
    uint8_t contract_address[20];
    uint8_t topics[4][32];  
    uint32_t topic_count;
    uint8_t data[256];
    size_t data_len;
    uint64_t block_number;
    uint32_t chain_id;
} l2_log_entry_t;

typedef struct {
    uint8_t log_hash[32];  
    uint8_t proof[32][32];  
    size_t proof_length;
    uint8_t receipts_root[32];  
} log_existence_proof_t;

typedef struct {
    
    l2_block_header_t block_headers[MAX_L2_CHAINS][MAX_BLOCK_HEADERS];
    size_t block_counts[MAX_L2_CHAINS];
    uint64_t latest_block_numbers[MAX_L2_CHAINS];
    
    
    bool is_syncing[MAX_L2_CHAINS];
    uint64_t sync_start_block[MAX_L2_CHAINS];
    uint64_t sync_end_block[MAX_L2_CHAINS];
    
    
    struct {
        uint8_t tx_hash[32];
        log_existence_proof_t proof;
        bool is_verified;
        uint32_t verified_by_tee;  
    } verification_cache[10000];
    size_t cache_count;
} l2_full_node_state_t;

int l2_full_node_init(l2_full_node_state_t* node);

int l2_full_node_sync_block_headers(l2_full_node_state_t* node,
                                    uint32_t chain_id,
                                    uint64_t from_block,
                                    uint64_t to_block);

bool l2_full_node_verify_log_existence(l2_full_node_state_t* node,
                                       const l2_log_entry_t* log,
                                       const log_existence_proof_t* proof);

int l2_full_node_distributed_verify_logs(l2_full_node_state_t* node,
                                         const l2_log_entry_t* logs,
                                         size_t log_count,
                                         const log_existence_proof_t* proofs,
                                         bool* verification_results,
                                         uint32_t* verified_by_tee);

int l2_full_node_get_block_header(l2_full_node_state_t* node,
                                  uint32_t chain_id,
                                  uint64_t block_number,
                                  l2_block_header_t* header);

void l2_full_node_compute_log_hash(const l2_log_entry_t* log, uint8_t* hash);

bool l2_full_node_verify_merkle_proof(const uint8_t* leaf_hash,
                                       const log_existence_proof_t* proof);

#endif
