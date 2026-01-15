#include "l2_full_node.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

#ifndef platform_malloc
#define platform_malloc malloc
#define platform_free free
#endif

int l2_full_node_init(l2_full_node_state_t* node) {
    if (node == NULL) return -1;
    
    memset(node, 0, sizeof(l2_full_node_state_t));
    
    
    for (size_t i = 0; i < MAX_L2_CHAINS; i++) {
        node->is_syncing[i] = false;
        node->latest_block_numbers[i] = 0;
        node->block_counts[i] = 0;
    }
    
    return 0;
}

int l2_full_node_sync_block_headers(l2_full_node_state_t* node,
                                    uint32_t chain_id,
                                    uint64_t from_block,
                                    uint64_t to_block) {
    if (node == NULL || chain_id >= MAX_L2_CHAINS) return -1;
    if (from_block > to_block) return -1;
    
    
    if (node->block_counts[chain_id] + (to_block - from_block + 1) > MAX_BLOCK_HEADERS) {
        return -1;  
    }
    
    
    node->is_syncing[chain_id] = true;
    node->sync_start_block[chain_id] = from_block;
    node->sync_end_block[chain_id] = to_block;
    
    
    for (uint64_t block_num = from_block; block_num <= to_block; block_num++) {
        size_t idx = node->block_counts[chain_id];
        if (idx >= MAX_BLOCK_HEADERS) break;
        
        l2_block_header_t* header = &node->block_headers[chain_id][idx];
        memset(header, 0, sizeof(l2_block_header_t));
        
        header->block_number = block_num;
        header->chain_id = chain_id;
        header->timestamp = block_num * 2;  
        
        
        uint8_t block_num_bytes[8];
        block_num_bytes[0] = (uint8_t)(block_num & 0xFF);
        block_num_bytes[1] = (uint8_t)((block_num >> 8) & 0xFF);
        block_num_bytes[2] = (uint8_t)((block_num >> 16) & 0xFF);
        block_num_bytes[3] = (uint8_t)((block_num >> 24) & 0xFF);
        block_num_bytes[4] = (uint8_t)((block_num >> 32) & 0xFF);
        block_num_bytes[5] = (uint8_t)((block_num >> 40) & 0xFF);
        block_num_bytes[6] = (uint8_t)((block_num >> 48) & 0xFF);
        block_num_bytes[7] = (uint8_t)((block_num >> 56) & 0xFF);
        
        platform_sha256(block_num_bytes, 8, header->block_hash);
        
        
        platform_sha256(block_num_bytes, 8, header->state_root);
        platform_sha256(block_num_bytes, 8, header->receipts_root);
        
        
        if (block_num > 0 && idx > 0) {
            memcpy(header->parent_hash, 
                   node->block_headers[chain_id][idx - 1].block_hash, 32);
        }
        
        node->block_counts[chain_id]++;
        node->latest_block_numbers[chain_id] = block_num;
    }
    
    node->is_syncing[chain_id] = false;
    
    return 0;
}

void l2_full_node_compute_log_hash(const l2_log_entry_t* log, uint8_t* hash) {
    if (log == NULL || hash == NULL) return;
    
    
    uint8_t buffer[1024];
    size_t offset = 0;
    
    
    memcpy(buffer + offset, log->tx_hash, 32);
    offset += 32;
    
    uint32_t log_index_bytes[1] = {log->log_index};
    memcpy(buffer + offset, log_index_bytes, 4);
    offset += 4;
    
    memcpy(buffer + offset, log->contract_address, 20);
    offset += 20;
    
    for (size_t i = 0; i < log->topic_count && i < 4; i++) {
        memcpy(buffer + offset, log->topics[i], 32);
        offset += 32;
    }
    
    memcpy(buffer + offset, log->data, log->data_len);
    offset += log->data_len;
    
    
    platform_sha256(buffer, offset, hash);
}

bool l2_full_node_verify_merkle_proof(const uint8_t* leaf_hash,
                                      const log_existence_proof_t* proof) {
    if (leaf_hash == NULL || proof == NULL) return false;
    
    uint8_t computed_hash[32];
    memcpy(computed_hash, leaf_hash, 32);
    
    
    for (size_t i = 0; i < proof->proof_length && i < 32; i++) {
        uint8_t buffer[64];
        
        
        if (memcmp(computed_hash, proof->proof[i], 32) < 0) {
            memcpy(buffer, computed_hash, 32);
            memcpy(buffer + 32, proof->proof[i], 32);
        } else {
            memcpy(buffer, proof->proof[i], 32);
            memcpy(buffer + 32, computed_hash, 32);
        }
        
        platform_sha256(buffer, 64, computed_hash);
    }
    
    
    return memcmp(computed_hash, proof->receipts_root, 32) == 0;
}

bool l2_full_node_verify_log_existence(l2_full_node_state_t* node,
                                       const l2_log_entry_t* log,
                                       const log_existence_proof_t* proof) {
    if (node == NULL || log == NULL || proof == NULL) return false;
    
    
    l2_block_header_t* header = NULL;
    for (size_t i = 0; i < node->block_counts[log->chain_id]; i++) {
        if (node->block_headers[log->chain_id][i].block_number == log->block_number) {
            header = &node->block_headers[log->chain_id][i];
            break;
        }
    }
    
    if (header == NULL) {
        
        return false;
    }
    
    
    if (memcmp(proof->receipts_root, header->receipts_root, 32) != 0) {
        return false;
    }
    
    
    uint8_t log_hash[32];
    l2_full_node_compute_log_hash(log, log_hash);
    
    
    return l2_full_node_verify_merkle_proof(log_hash, proof);
}

int l2_full_node_distributed_verify_logs(l2_full_node_state_t* node,
                                         const l2_log_entry_t* logs,
                                         size_t log_count,
                                         const log_existence_proof_t* proofs,
                                         bool* verification_results,
                                         uint32_t* verified_by_tee) {
    if (node == NULL || logs == NULL || proofs == NULL || 
        verification_results == NULL) return -1;
    
    
    memset(verification_results, 0, log_count * sizeof(bool));
    if (verified_by_tee != NULL) {
        memset(verified_by_tee, 0, log_count * sizeof(uint32_t));
    }
    
    
    for (size_t i = 0; i < log_count; i++) {
        
        bool found_in_cache = false;
        for (size_t j = 0; j < node->cache_count && j < 10000; j++) {
            if (memcmp(node->verification_cache[j].tx_hash, logs[i].tx_hash, 32) == 0) {
                if (node->verification_cache[j].is_verified) {
                    verification_results[i] = true;
                    if (verified_by_tee != NULL) {
                        verified_by_tee[i] = node->verification_cache[j].verified_by_tee;
                    }
                    found_in_cache = true;
                    break;
                }
            }
        }
        
        if (!found_in_cache) {
            
            verification_results[i] = l2_full_node_verify_log_existence(
                node, &logs[i], &proofs[i]);
            
            
            if (node->cache_count < 10000) {
                memcpy(node->verification_cache[node->cache_count].tx_hash, 
                       logs[i].tx_hash, 32);
                memcpy(&node->verification_cache[node->cache_count].proof, 
                       &proofs[i], sizeof(log_existence_proof_t));
                node->verification_cache[node->cache_count].is_verified = verification_results[i];
                node->verification_cache[node->cache_count].verified_by_tee = 0;  
                node->cache_count++;
            }
        }
    }
    
    return 0;
}

int l2_full_node_get_block_header(l2_full_node_state_t* node,
                                  uint32_t chain_id,
                                  uint64_t block_number,
                                  l2_block_header_t* header) {
    if (node == NULL || header == NULL || chain_id >= MAX_L2_CHAINS) return -1;
    
    
    for (size_t i = 0; i < node->block_counts[chain_id]; i++) {
        if (node->block_headers[chain_id][i].block_number == block_number) {
            memcpy(header, &node->block_headers[chain_id][i], sizeof(l2_block_header_t));
            return 0;
        }
    }
    
    return -1;  
}
