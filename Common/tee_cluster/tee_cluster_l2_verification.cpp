

#include "tee_cluster.h"
#include "../l2_full_node/l2_full_node.h"
#include "../merkle_crdt/merkle_crdt.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <map>

typedef struct {
    uint32_t tee_node_id;           
    l2_log_entry_t log;             
    log_existence_proof_t proof;     
    bool is_verified;               
    bool is_assigned;               
} distributed_verification_task_t;

int tee_cluster_init_l2_full_node(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    l2_full_node_state_t* l2_node = (l2_full_node_state_t*)platform_malloc(sizeof(l2_full_node_state_t));
    if (l2_node == NULL) return -1;
    
    if (l2_full_node_init(l2_node) != 0) {
        platform_free(l2_node);
        return -1;
    }
    
    cluster->l2_full_node_state = l2_node;
    
    return 0;
}

int tee_cluster_sync_l2_block_headers(tee_cluster_state_t* cluster,
                                       uint32_t chain_id,
                                       uint64_t from_block,
                                       uint64_t to_block) {
    if (cluster == NULL || cluster->l2_full_node_state == NULL) return -1;
    
    l2_full_node_state_t* l2_node = (l2_full_node_state_t*)cluster->l2_full_node_state;
    
    return l2_full_node_sync_block_headers(l2_node, chain_id, from_block, to_block);
}

int tee_cluster_distributed_verify_logs_merkle_crdt(
    tee_cluster_state_t* cluster,
    const void* logs,  
    size_t log_count,
    const void* proofs,  
    uint32_t tee_node_count,
    bool* verification_results) {
    
    if (cluster == NULL || logs == NULL || proofs == NULL || 
        verification_results == NULL || log_count == 0) return -1;
    
    if (cluster->l2_full_node_state == NULL) {
        
        if (tee_cluster_init_l2_full_node(cluster) != 0) {
            return -1;
        }
    }
    
    l2_full_node_state_t* l2_node = (l2_full_node_state_t*)cluster->l2_full_node_state;
    
    
    const l2_log_entry_t* log_array = (const l2_log_entry_t*)logs;
    const log_existence_proof_t* proof_array = (const log_existence_proof_t*)proofs;
    
    
    
    
    std::vector<distributed_verification_task_t> tasks(log_count);
    
    
    for (size_t i = 0; i < log_count; i++) {
        tasks[i].tee_node_id = (uint32_t)(i % tee_node_count);  
        memcpy(&tasks[i].log, &log_array[i], sizeof(l2_log_entry_t));
        memcpy(&tasks[i].proof, &proof_array[i], sizeof(log_existence_proof_t));
        tasks[i].is_verified = false;
        tasks[i].is_assigned = false;
    }
    
    
    std::map<uint32_t, std::vector<size_t>> tee_tasks;
    for (size_t i = 0; i < log_count; i++) {
        uint32_t tee_id = tasks[i].tee_node_id;
        tee_tasks[tee_id].push_back(i);
    }
    
    
    
    
    
    for (const auto& pair : tee_tasks) {
          
        const std::vector<size_t>& task_indices = pair.second;
        
        
        l2_log_entry_t* tee_logs = (l2_log_entry_t*)platform_malloc(
            task_indices.size() * sizeof(l2_log_entry_t));
        log_existence_proof_t* tee_proofs = (log_existence_proof_t*)platform_malloc(
            task_indices.size() * sizeof(log_existence_proof_t));
        bool* tee_results = (bool*)platform_malloc(
            task_indices.size() * sizeof(bool));
        uint32_t* verified_by = (uint32_t*)platform_malloc(
            task_indices.size() * sizeof(uint32_t));
        
        if (tee_logs == NULL || tee_proofs == NULL || tee_results == NULL || verified_by == NULL) {
            if (tee_logs) platform_free(tee_logs);
            if (tee_proofs) platform_free(tee_proofs);
            if (tee_results) platform_free(tee_results);
            if (verified_by) platform_free(verified_by);
            continue;
        }
        
        
        for (size_t j = 0; j < task_indices.size(); j++) {
            size_t task_idx = task_indices[j];
            memcpy(&tee_logs[j], &log_array[task_idx], sizeof(l2_log_entry_t));
            memcpy(&tee_proofs[j], &proof_array[task_idx], sizeof(log_existence_proof_t));
        }
        
        
        l2_full_node_distributed_verify_logs(l2_node, tee_logs, task_indices.size(),
                                            tee_proofs, tee_results, verified_by);
        
        
        for (size_t j = 0; j < task_indices.size(); j++) {
            size_t task_idx = task_indices[j];
            verification_results[task_idx] = tee_results[j];
            tasks[task_idx].is_verified = tee_results[j];
            tasks[task_idx].is_assigned = true;
        }
        
        platform_free(tee_logs);
        platform_free(tee_proofs);
        platform_free(tee_results);
        platform_free(verified_by);
    }
    
    return 0;
}

bool tee_cluster_verify_log_existence(tee_cluster_state_t* cluster,
                                      const void* log,  
                                      const void* proof) {  
    if (cluster == NULL || log == NULL || proof == NULL) return false;
    
    if (cluster->l2_full_node_state == NULL) {
        
        if (tee_cluster_init_l2_full_node(cluster) != 0) {
            return false;
        }
    }
    
    l2_full_node_state_t* l2_node = (l2_full_node_state_t*)cluster->l2_full_node_state;
    
    const l2_log_entry_t* log_entry = (const l2_log_entry_t*)log;
    const log_existence_proof_t* proof_entry = (const log_existence_proof_t*)proof;
    
    return l2_full_node_verify_log_existence(l2_node, log_entry, proof_entry);
}

int tee_cluster_process_log_with_verification(tee_cluster_state_t* cluster,
                                              uint32_t chain_id,
                                              const l2_log_entry_t* log,
                                              const log_existence_proof_t* proof,
                                              operation_t* op) {
    if (cluster == NULL || log == NULL || proof == NULL || op == NULL) return -1;
    
    
    if (!tee_cluster_verify_log_existence(cluster, log, proof)) {
        return -1;  
    }
    
    
    
    
    
    memset(op, 0, sizeof(operation_t));
    op->tx_id = *(uint64_t*)log->tx_hash;  
    op->timestamp = log->block_number * 2;  
    
    
    
    if (log->topic_count > 0) {
        
        memcpy(op->token_address, log->topics[0], 32);
    }
    
    if (log->topic_count > 1) {
        
        memcpy(op->account, log->topics[1], 20);
    }
    
    if (log->data_len >= 32) {
        
        memcpy(op->amount, log->data, 32);
    }
    
    
    
    if (log->topic_count >= 3) {
        op->type = OP_SUBTRACT;  
    } else {
        op->type = OP_ADD;  
    }
    
    op->is_valid = true;
    
    return 0;
}
