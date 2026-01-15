#include "sequencer.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

int sequencer_init(sequencer_state_t* state) {
    if (state == NULL) return -1;
    
    memset(state, 0, sizeof(sequencer_state_t));
    
    
    for (size_t i = 0; i < MAX_NODES; i++) {
        if (mpt_tree_init(&state->token_trees[i]) != 0) {
            return -1;
        }
    }
    
    state->next_sequence_id = 1;
    state->token_count = 0;
    state->log_count = 0;
    state->node_count = 0;
    state->current_leader = 0;
    
    return 0;
}

static int get_or_create_token_tree(sequencer_state_t* state,
                                     const uint8_t* token_address,
                                     mpt_tree_t** tree) {
    if (state == NULL || token_address == NULL || tree == NULL) return -1;
    
    
    for (size_t i = 0; i < state->token_count; i++) {
        if (memcmp(state->token_addresses[i], token_address, MAX_TOKEN_ADDRESS_LEN) == 0) {
            *tree = &state->token_trees[i];
            return 0;
        }
    }
    
    
    if (state->token_count >= MAX_NODES) return -1;
    
    size_t idx = state->token_count;
    memcpy(state->token_addresses[idx], token_address, MAX_TOKEN_ADDRESS_LEN);
    *tree = &state->token_trees[idx];
    state->token_count++;
    
    return 0;
}

int sequencer_add_log(sequencer_state_t* state, const log_entry_t* log) {
    if (state == NULL || log == NULL) return -1;
    
    if (state->log_count >= MAX_LOG_ENTRIES) return -1;
    
    
    memcpy(&state->log_queue[state->log_count], log, sizeof(log_entry_t));
    
    
    state->log_queue[state->log_count].sequence_id = state->next_sequence_id++;
    state->log_queue[state->log_count].processed = false;
    
    state->log_count++;
    
    return 0;
}

static int compare_logs(const void* a, const void* b) {
    const log_entry_t* log_a = (const log_entry_t*)a;
    const log_entry_t* log_b = (const log_entry_t*)b;
    
    
    if (log_a->timestamp < log_b->timestamp) return -1;
    if (log_a->timestamp > log_b->timestamp) return 1;
    
    
    if (log_a->sequence_id < log_b->sequence_id) return -1;
    if (log_a->sequence_id > log_b->sequence_id) return 1;
    
    return 0;
}

int sequencer_process_logs(sequencer_state_t* state) {
    if (state == NULL) return -1;
    
    
    log_entry_t* unprocessed_logs = (log_entry_t*)platform_malloc(state->log_count * sizeof(log_entry_t));
    if (unprocessed_logs == NULL) return -1;
    
    size_t unprocessed_count = 0;
    for (size_t i = 0; i < state->log_count; i++) {
        if (!state->log_queue[i].processed) {
            memcpy(&unprocessed_logs[unprocessed_count], &state->log_queue[i], sizeof(log_entry_t));
            unprocessed_count++;
        }
    }
    
    if (unprocessed_count == 0) {
        free(unprocessed_logs);
        return 0;
    }
    
    
    qsort(unprocessed_logs, unprocessed_count, sizeof(log_entry_t), compare_logs);
    
    
    for (size_t i = 0; i < unprocessed_count; i++) {
        log_entry_t* log = &unprocessed_logs[i];
        
        
        if (!sequencer_verify_log_signature(log)) {
            
            continue;
        }
        
        
        mpt_tree_t* tree = NULL;
        if (get_or_create_token_tree(state, log->token_address, &tree) != 0) {
            continue;
        }
        
        
        switch (log->type) {
            case LOG_TRANSFER: {
                
                uint8_t from_key[64];
                memcpy(from_key, log->from, 20);
                memcpy(from_key + 20, log->token_address, MAX_TOKEN_ADDRESS_LEN);
                
                uint8_t from_balance[32];
                size_t balance_len = 32;
                if (mpt_tree_get(tree, from_key, 52, from_balance, &balance_len) == 0) {
                    
                    
                } else {
                    
                    uint8_t zero_balance[32] = {0};
                    mpt_tree_insert(tree, from_key, 52, zero_balance, 32);
                }
                
                
                uint8_t to_key[64];
                memcpy(to_key, log->to, 20);
                memcpy(to_key + 20, log->token_address, MAX_TOKEN_ADDRESS_LEN);
                
                uint8_t to_balance[32];
                balance_len = 32;
                if (mpt_tree_get(tree, to_key, 52, to_balance, &balance_len) == 0) {
                    
                    
                } else {
                    
                    memcpy(to_balance, log->amount, 32);
                    mpt_tree_insert(tree, to_key, 52, to_balance, 32);
                }
                
                break;
            }
            
            case LOG_MINT: {
                
                uint8_t to_key[64];
                memcpy(to_key, log->to, 20);
                memcpy(to_key + 20, log->token_address, MAX_TOKEN_ADDRESS_LEN);
                
                uint8_t to_balance[32];
                size_t balance_len = 32;
                if (mpt_tree_get(tree, to_key, 52, to_balance, &balance_len) == 0) {
                    
                } else {
                    memcpy(to_balance, log->amount, 32);
                    mpt_tree_insert(tree, to_key, 52, to_balance, 32);
                }
                break;
            }
            
            case LOG_BURN: {
                
                uint8_t from_key[64];
                memcpy(from_key, log->from, 20);
                memcpy(from_key + 20, log->token_address, MAX_TOKEN_ADDRESS_LEN);
                
                uint8_t from_balance[32];
                size_t balance_len = 32;
                if (mpt_tree_get(tree, from_key, 52, from_balance, &balance_len) == 0) {
                    
                }
                break;
            }
            
            default:
                break;
        }
        
        
        for (size_t j = 0; j < state->log_count; j++) {
            if (state->log_queue[j].sequence_id == log->sequence_id) {
                state->log_queue[j].processed = true;
                break;
            }
        }
    }
    
    free(unprocessed_logs);
    return 0;
}

int sequencer_get_token_root(sequencer_state_t* state,
                              const uint8_t* token_address,
                              uint8_t* root_hash) {
    if (state == NULL || token_address == NULL || root_hash == NULL) return -1;
    
    mpt_tree_t* tree = NULL;
    if (get_or_create_token_tree(state, token_address, &tree) != 0) {
        return -1;
    }
    
    return mpt_tree_get_root_hash(tree, root_hash);
}

int sequencer_get_balance(sequencer_state_t* state,
                           const uint8_t* token_address,
                           const uint8_t* account,
                           uint8_t* balance) {
    if (state == NULL || token_address == NULL || account == NULL || balance == NULL) {
        return -1;
    }
    
    mpt_tree_t* tree = NULL;
    if (get_or_create_token_tree(state, token_address, &tree) != 0) {
        return -1;
    }
    
    uint8_t key[64];
    memcpy(key, account, 20);
    memcpy(key + 20, token_address, MAX_TOKEN_ADDRESS_LEN);
    
    size_t balance_len = 32;
    return mpt_tree_get(tree, key, 52, balance, &balance_len);
}

bool sequencer_verify_log_signature(const log_entry_t* log) {
    if (log == NULL) return false;
    
    
    
    bool all_zero = true;
    for (int i = 0; i < 65; i++) {
        if (log->signature[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    return !all_zero;
}
