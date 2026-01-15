#include "tee_cluster.h"
#include "../tee_network/tee_network.h"
#include "sequencer.h"
#include "../mpt_tree/mpt_tree_common.h"
#include "../raft/raft.h"
#include "../raft/raft_network.h"
#include "../merkle_crdt/merkle_crdt.h"
#include <string.h>
#include <stdlib.h>
#include <set>
#include <map>

static int get_hw_random(uint64_t* random) {
    if (random == NULL) return -1;
    
    return platform_get_random((uint8_t*)random, sizeof(uint64_t));
}

int tee_cluster_init(tee_cluster_state_t* cluster, uint32_t node_id) {
    if (cluster == NULL) return -1;
    
    memset(cluster, 0, sizeof(tee_cluster_state_t));
    cluster->my_node_id = node_id;
    cluster->current_leader = 0;
    cluster->last_leader_election = 0;
    
    
    if (mpt_tree_init(&cluster->token_registry) != 0) {
        return -1;
    }
    
    
    for (size_t i = 0; i < MAX_NODES; i++) {
        if (mpt_tree_init(&cluster->token_trees[i]) != 0) {
            return -1;
        }
    }
    
    
    if (merkle_crdt_init(&cluster->global_dag) != 0) {
        return -1;
    }
    
    
    
    
    if (tee_network_init(&cluster->network, node_id, "0.0.0.0", 8080) != 0) {
        return -1;
    }
    
    
    if (raft_init(&cluster->raft, node_id) != 0) {
        return -1;
    }
    
    
    cluster->current_epoch = 0;
    cluster->epoch_start_time = 0;
    cluster->epoch_in_progress = false;
    
    
    cluster->epoch_output_count = 0;
    cluster->epoch_output_collected = false;
    cluster->tx_sort_count = 0;
    cluster->executed_count = 0;
    
    return 0;
}

int tee_cluster_register_token(tee_cluster_state_t* cluster,
                                const uint8_t* token_address,
                                const uint8_t* chain_id,
                                const uint8_t* deploy_tx_hash) {
    if (cluster == NULL || token_address == NULL) return -1;
    
    
    uint8_t key[64];
    memcpy(key, chain_id, 4);  
    memcpy(key + 4, token_address, 42);
    
    
    uint8_t value[32];
    memcpy(value, deploy_tx_hash, 32);
    
    
    int ret = mpt_tree_insert(&cluster->token_registry, key, 46, value, 32);
    if (ret != 0) return ret;
    
    
    if (cluster->token_count < MAX_NODES) {
        memcpy(cluster->token_addresses[cluster->token_count], token_address, 42);
        cluster->token_count++;
    }
    
    return 0;
}

int tee_cluster_add_tx_request(tee_cluster_state_t* cluster,
                                const tx_request_t* tx) {
    if (cluster == NULL || tx == NULL) return -1;
    
    if (cluster->pending_count >= MAX_PENDING_TXS) return -1;
    
    memcpy(&cluster->pending_txs[cluster->pending_count], tx, sizeof(tx_request_t));
    cluster->pending_count++;
    
    return 0;
}

int tee_cluster_elect_leader(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    raft_tick_with_network(&cluster->raft, &cluster->network);
    
    
    uint32_t raft_leader = raft_get_leader(&cluster->raft);
    if (raft_leader != cluster->current_leader) {
        cluster->current_leader = raft_leader;
        
        
        for (size_t i = 0; i < cluster->node_count; i++) {
            cluster->nodes[i].is_leader = (cluster->nodes[i].node_id == raft_leader);
        }
    }
    
    return 0;
}

static int compare_txs(const void* a, const void* b) {
    const tx_request_t* tx_a = (const tx_request_t*)a;
    const tx_request_t* tx_b = (const tx_request_t*)b;
    
    
    if (tx_a->timestamp < tx_b->timestamp) return -1;
    if (tx_a->timestamp > tx_b->timestamp) return 1;
    
    
    uint64_t random_a, random_b;
    if (get_hw_random(&random_a) != 0 || get_hw_random(&random_b) != 0) {
        
        if (tx_a->tx_id < tx_b->tx_id) return -1;
        if (tx_a->tx_id > tx_b->tx_id) return 1;
        return 0;
    }
    
    if (random_a < random_b) return -1;
    if (random_a > random_b) return 1;
    
    return 0;
}

int tee_cluster_sort_txs(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    bool is_leader = false;
    for (size_t i = 0; i < cluster->node_count; i++) {
        if (cluster->nodes[i].node_id == cluster->my_node_id &&
            cluster->nodes[i].is_leader) {
            is_leader = true;
            break;
        }
    }
    
    if (!is_leader) {
        
        return 0;
    }
    
    
    memcpy(cluster->sorted_txs, cluster->pending_txs, 
           cluster->pending_count * sizeof(tx_request_t));
    cluster->sorted_count = cluster->pending_count;
    
    
    qsort(cluster->sorted_txs, cluster->sorted_count, 
          sizeof(tx_request_t), compare_txs);
    
    
    for (size_t i = 0; i < cluster->sorted_count; i++) {
        cluster->sorted_txs[i].is_processed = true;
    }
    
    
    uint8_t broadcast_payload[sizeof(tx_request_t) * MAX_PENDING_TXS];
    size_t payload_size = cluster->sorted_count * sizeof(tx_request_t);
    memcpy(broadcast_payload, cluster->sorted_txs, payload_size);
    
    tee_network_broadcast(&cluster->network, MSG_SORTED_TXS,
                          broadcast_payload, payload_size);
    
    return 0;
}

static merkle_crdt_dag_t* get_global_dag(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return NULL;
    return &cluster->global_dag;
}

int tee_cluster_process_operation(tee_cluster_state_t* cluster,
                                   uint32_t chain_id,
                                   const operation_t* op) {
    if (cluster == NULL || op == NULL) return -1;
    
    
    merkle_crdt_dag_t* dag = get_global_dag(cluster);
    if (dag == NULL) return -1;
    
    
    uint64_t tx_sort_order = op->tx_id;  
    
    
    for (size_t i = 0; i < cluster->sorted_count; i++) {
        if (cluster->sorted_txs[i].tx_id == op->tx_id) {
            tx_sort_order = i;  
            break;
        }
    }
    
    
    
    int ret = merkle_crdt_add_operation(dag, op, tx_sort_order);
    if (ret != 0) return ret;
    
    
    dag_node_t* new_node = NULL;
    for (size_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i]->operation.operation_id == op->operation_id) {
            new_node = dag->nodes[i];
            break;
        }
    }
    
    if (new_node == NULL) return -1;
    
    
    
    mpt_tree_t* token_tree = NULL;
    for (size_t i = 0; i < cluster->token_count; i++) {
        if (memcmp(cluster->token_addresses[i], op->token_address, 42) == 0) {
            token_tree = &cluster->token_trees[i];
            break;
        }
    }
    
    if (token_tree == NULL) {
        
        if (cluster->token_count >= MAX_NODES) return -1;
        size_t idx = cluster->token_count;
        memcpy(cluster->token_addresses[idx], op->token_address, 42);
        mpt_tree_init(&cluster->token_trees[idx]);
        token_tree = &cluster->token_trees[idx];
        cluster->token_count++;
    }
    
    
    merkle_crdt_update_parent_states(dag, new_node, token_tree);
    
    
    operation_t tx_operations[100];
    size_t tx_op_count = 0;
    
    
    if (merkle_crdt_collect_tx_operations(dag, op->tx_id, tx_operations, &tx_op_count, 100) == 0) {
        
        
        
        
        
        if (tx_op_count >= 2) {
            
            bool tx_failed = !merkle_crdt_validate_tx(tx_operations, tx_op_count, token_tree);
            
            
            for (size_t i = 0; i < dag->node_count; i++) {
                if (dag->nodes[i]->operation.tx_id == op->tx_id) {
                    
                    bool node_failed = merkle_crdt_check_operation_failed(dag, dag->nodes[i], token_tree);
                    dag->nodes[i]->is_failed = tx_failed || node_failed;
                    
                    if (dag->nodes[i]->is_failed) {
                        dag->nodes[i]->operation.is_valid = false;
                    }
                }
            }
            
            if (tx_failed) {
                return -1; 
            }
        }
        
    }
    
    return 0;
}

int tee_cluster_process_operation_serial(tee_cluster_state_t* cluster,
                                         const operation_t* op,
                                         operation_t* tx_ops_cache,
                                         size_t* tx_ops_cache_count,
                                         size_t max_cache_size) {
    if (cluster == NULL || op == NULL) return -1;
    
    
    mpt_tree_t* token_tree = NULL;
    for (size_t i = 0; i < cluster->token_count; i++) {
        if (memcmp(cluster->token_addresses[i], op->token_address, 42) == 0) {
            token_tree = &cluster->token_trees[i];
            break;
        }
    }
    
    if (token_tree == NULL) {
        
        if (cluster->token_count >= MAX_NODES) return -1;
        size_t idx = cluster->token_count;
        memcpy(cluster->token_addresses[idx], op->token_address, 42);
        mpt_tree_init(&cluster->token_trees[idx]);
        token_tree = &cluster->token_trees[idx];
        cluster->token_count++;
    }
    
    
    if (tx_ops_cache != NULL && tx_ops_cache_count != NULL && 
        *tx_ops_cache_count < max_cache_size) {
        memcpy(&tx_ops_cache[*tx_ops_cache_count], op, sizeof(operation_t));
        (*tx_ops_cache_count)++;
    }
    
    
    uint8_t key[64];
    memcpy(key, op->account, 20);
    memcpy(key + 20, op->token_address, 42);
    
    
    uint8_t current_balance[32];
    size_t balance_len = 32;
    int ret = mpt_tree_get(token_tree, key, 64, current_balance, &balance_len);
    
    uint8_t new_balance[32];
    memset(new_balance, 0, 32);
    
    if (ret == 0 && balance_len == 32) {
        
        memcpy(new_balance, current_balance, 32);
        
        
        if (op->type == OP_ADD) {
            
            bool carry = false;
            for (int j = 31; j >= 0; j--) {
                uint16_t sum = (uint16_t)new_balance[j] + 
                               (uint16_t)op->amount[j] + 
                               (carry ? 1 : 0);
                new_balance[j] = (uint8_t)(sum & 0xFF);
                carry = (sum > 0xFF);
            }
        } else if (op->type == OP_SUBTRACT) {
            
            bool borrow = false;
            for (int j = 31; j >= 0; j--) {
                int16_t diff = (int16_t)new_balance[j] - 
                               (int16_t)op->amount[j] - 
                               (borrow ? 1 : 0);
                if (diff < 0) {
                    diff += 256;
                    borrow = true;
                } else {
                    borrow = false;
                }
                new_balance[j] = (uint8_t)(diff & 0xFF);
            }
            
            
            if (borrow) {
                
                return -1;
            }
        } else if (op->type == OP_SET) {
            
            memcpy(new_balance, op->amount, 32);
        }
    } else {
        
        if (op->type == OP_ADD || op->type == OP_SET) {
            memcpy(new_balance, op->amount, 32);
        } else if (op->type == OP_SUBTRACT) {
            
            return -1;
        }
    }
    
    
    mpt_tree_insert(token_tree, key, 64, new_balance, 32);
    
    return 0;
}

int tee_cluster_process_operations_serial_with_validation(tee_cluster_state_t* cluster,
                                                          const operation_t* operations,
                                                          size_t op_count) {
    if (cluster == NULL || operations == NULL || op_count == 0) return -1;
    
    
    
    
    size_t i = 0;
    while (i < op_count) {
        uint64_t current_tx_id = operations[i].tx_id;
        operation_t tx_ops[100];
        size_t tx_op_count = 0;
        
        
        while (i < op_count && operations[i].tx_id == current_tx_id && tx_op_count < 100) {
            memcpy(&tx_ops[tx_op_count], &operations[i], sizeof(operation_t));
            tx_op_count++;
            i++;
        }
        
        
        mpt_tree_t* token_tree = NULL;
        if (tx_op_count > 0) {
            for (size_t j = 0; j < cluster->token_count; j++) {
                if (memcmp(cluster->token_addresses[j], tx_ops[0].token_address, 42) == 0) {
                    token_tree = &cluster->token_trees[j];
                    break;
                }
            }
            
            if (token_tree == NULL) {
                
                if (cluster->token_count >= MAX_NODES) continue;
                size_t idx = cluster->token_count;
                memcpy(cluster->token_addresses[idx], tx_ops[0].token_address, 42);
                mpt_tree_init(&cluster->token_trees[idx]);
                token_tree = &cluster->token_trees[idx];
                cluster->token_count++;
            }
        }
        
        
        if (token_tree != NULL && tx_op_count >= 2) {
            if (!merkle_crdt_validate_tx(tx_ops, tx_op_count, token_tree)) {
                
                continue;
            }
        }
        
        
        for (size_t j = 0; j < tx_op_count; j++) {
            const operation_t* op = &tx_ops[j];
            
            
            uint8_t key[64];
            memcpy(key, op->account, 20);
            memcpy(key + 20, op->token_address, 42);
            
            
            uint8_t current_balance[32];
            size_t balance_len = 32;
            int ret = mpt_tree_get(token_tree, key, 64, current_balance, &balance_len);
            
            uint8_t new_balance[32];
            memset(new_balance, 0, 32);
            
            if (ret == 0 && balance_len == 32) {
                
                memcpy(new_balance, current_balance, 32);
                
                
                if (op->type == OP_ADD) {
                    
                    bool carry = false;
                    for (int k = 31; k >= 0; k--) {
                        uint16_t sum = (uint16_t)new_balance[k] + 
                                       (uint16_t)op->amount[k] + 
                                       (carry ? 1 : 0);
                        new_balance[k] = (uint8_t)(sum & 0xFF);
                        carry = (sum > 0xFF);
                    }
                } else if (op->type == OP_SUBTRACT) {
                    
                    bool borrow = false;
                    for (int k = 31; k >= 0; k--) {
                        int16_t diff = (int16_t)new_balance[k] - 
                                       (int16_t)op->amount[k] - 
                                       (borrow ? 1 : 0);
                        if (diff < 0) {
                            diff += 256;
                            borrow = true;
                        } else {
                            borrow = false;
                        }
                        new_balance[k] = (uint8_t)(diff & 0xFF);
                    }
                    
                    
                    if (borrow) {
                        
                        break;
                    }
                } else if (op->type == OP_SET) {
                    
                    memcpy(new_balance, op->amount, 32);
                }
            } else {
                
                if (op->type == OP_ADD || op->type == OP_SET) {
                    memcpy(new_balance, op->amount, 32);
                } else if (op->type == OP_SUBTRACT) {
                    
                    break;
                }
            }
            
            
            mpt_tree_insert(token_tree, key, 64, new_balance, 32);
        }
    }
    
    return 0;
}

int tee_cluster_broadcast_dag_node(tee_cluster_state_t* cluster,
                                    uint32_t chain_id,
                                    uint64_t node_id) {
    if (cluster == NULL) return -1;
    
    
    merkle_crdt_dag_t* dag = get_global_dag(cluster);
    if (dag == NULL) return -1;
    
    
    dag_node_t* node = NULL;
    for (size_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i]->node_id == node_id) {
            node = dag->nodes[i];
            break;
        }
    }
    
    if (node == NULL) return -1;
    
    
    if (dag->latest_count < MAX_DAG_NODES) {
        dag->latest_nodes[dag->latest_count++] = node;
    }
    
    
    uint8_t broadcast_payload[1024];
    size_t offset = 0;
    
    memcpy(broadcast_payload + offset, &chain_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(broadcast_payload + offset, &node_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(broadcast_payload + offset, &node->operation, sizeof(operation_t));
    offset += sizeof(operation_t);
    memcpy(broadcast_payload + offset, node->merkle_hash, 32);
    offset += 32;
    
    
    tee_network_broadcast(&cluster->network, MSG_DAG_NODE, 
                          broadcast_payload, offset);
    
    return 0;
}

int tee_cluster_request_dag_node(tee_cluster_state_t* cluster,
                                  uint32_t chain_id,
                                  uint64_t node_id) {
    if (cluster == NULL) return -1;
    
    
    merkle_crdt_dag_t* dag = get_global_dag(cluster);
    if (dag == NULL) return -1;
    
    for (size_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i]->node_id == node_id) {
            return 0; 
        }
    }
    
    
    uint8_t request_payload[16];
    size_t offset = 0;
    
    memcpy(request_payload + offset, &chain_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(request_payload + offset, &node_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    tee_network_broadcast(&cluster->network, MSG_REQUEST_DAG_NODE,
                          request_payload, offset);
    
    return -1; 
}

int tee_cluster_sync_dag(tee_cluster_state_t* cluster, uint32_t chain_id) {
    if (cluster == NULL) return -1;
    
    merkle_crdt_dag_t* dag = get_global_dag(cluster);
    if (dag == NULL) return -1;
    
    
    for (size_t i = 0; i < dag->latest_count; i++) {
        dag_node_t* node = dag->latest_nodes[i];
        
        
        bool node_exists = false;
        for (size_t j = 0; j < dag->node_count; j++) {
            if (dag->nodes[j]->node_id == node->node_id) {
                node_exists = true;
                break;
            }
        }
        
        if (!node_exists) {
            
            tee_cluster_request_dag_node(cluster, chain_id, node->node_id);
        }
        
        
        for (size_t j = 0; j < node->parent_count; j++) {
            bool parent_exists = false;
            for (size_t k = 0; k < dag->node_count; k++) {
                if (dag->nodes[k]->node_id == node->parents[j]->node_id) {
                    parent_exists = true;
                    break;
                }
            }
            
            if (!parent_exists) {
                
                tee_cluster_request_dag_node(cluster, chain_id, 
                                             node->parents[j]->node_id);
            }
        }
    }
    
    return 0;
}

int tee_cluster_periodic_broadcast(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    merkle_crdt_dag_t* dag = &cluster->global_dag;
    
    
    for (size_t j = 0; j < dag->latest_count; j++) {
        dag_node_t* node = dag->latest_nodes[j];
        
        uint32_t chain_id = 0;  
        tee_cluster_broadcast_dag_node(cluster, chain_id, node->node_id);
    }
    
    
    dag->latest_count = 0;
    
    return 0;
}

int tee_cluster_generate_epoch_output(tee_cluster_state_t* cluster,
                                       uint8_t* mpt_root,
                                       uint8_t* dag_head,
                                       uint8_t* reject_root) {
    if (cluster == NULL) return -1;
    if (mpt_root == NULL || dag_head == NULL || reject_root == NULL) return -1;
    
    
    merkle_crdt_dag_t* dag = &cluster->global_dag;
    
    
    merkle_crdt_generate_head(dag);
    
    
    
    if (dag->head != NULL && cluster->token_count > 0) {
        
        
        for (size_t i = 0; i < dag->head->child_count; i++) {
            dag_node_t* node = dag->head->children[i];
            
            mpt_tree_t* token_tree = NULL;
            for (size_t j = 0; j < cluster->token_count; j++) {
                if (memcmp(cluster->token_addresses[j], node->operation.token_address, 42) == 0) {
                    token_tree = &cluster->token_trees[j];
                    break;
                }
            }
            if (token_tree != NULL) {
                merkle_crdt_update_state(dag, token_tree);
            }
        }
    }
    
    
    uint8_t all_roots[32 * MAX_NODES];
    size_t root_count = 0;
    
    for (size_t i = 0; i < cluster->token_count; i++) {
        uint8_t token_root[32];
        if (mpt_tree_get_root_hash(&cluster->token_trees[i], token_root) == 0) {
            memcpy(all_roots + root_count * 32, token_root, 32);
            root_count++;
        }
    }
    
    
    if (root_count > 0) {
        platform_sha256(all_roots, root_count * 32, mpt_root);
    } else {
        memset(mpt_root, 0, 32);
    }
    
    
    if (dag->head != NULL) {
        memcpy(dag_head, dag->head_hash, 32);
    } else {
        memset(dag_head, 0, 32);
    }
    
    
    
    uint8_t failed_nodes_hash[32];
    memset(failed_nodes_hash, 0, 32);
    
    
    uint8_t temp_buffer[4096];
    size_t temp_offset = 0;
    for (size_t i = 0; i < dag->node_count && temp_offset < 4096 - 32; i++) {
        if (dag->nodes[i]->is_failed) {
            memcpy(temp_buffer + temp_offset, dag->nodes[i]->merkle_hash, 32);
            temp_offset += 32;
        }
    }
    
    
    if (temp_offset > 0) {
        platform_sha256(temp_buffer, temp_offset, failed_nodes_hash);
    }
    memcpy(reject_root, failed_nodes_hash, 32);
    
    return 0;
}

int tee_cluster_start_epoch(tee_cluster_state_t* cluster, uint64_t epoch_id) {
    if (cluster == NULL) return -1;
    
    if (cluster->epoch_in_progress) {
        return -1; 
    }
    
    
    if (raft_start_epoch(&cluster->raft, epoch_id) != 0) {
        return -1;
    }
    
    cluster->current_epoch = epoch_id;
    cluster->epoch_start_time = 0; 
    cluster->epoch_in_progress = true;
    
    return 0;
}

int tee_cluster_end_epoch(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    if (!cluster->epoch_in_progress) {
        return -1; 
    }
    
    
    if (raft_end_epoch(&cluster->raft) != 0) {
        return -1;
    }
    
    cluster->epoch_in_progress = false;
    
    return 0;
}

bool tee_cluster_is_epoch_complete(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return false;
    
    
    return raft_is_epoch_complete(&cluster->raft);
}

int tee_cluster_set_tx_sort_info(tee_cluster_state_t* cluster,
                                  const tx_sort_info_t* sort_info,
                                  size_t count) {
    if (cluster == NULL || sort_info == NULL) return -1;
    
    if (count > MAX_PENDING_TXS) return -1;
    
    
    for (size_t i = 0; i < count; i++) {
        if (cluster->tx_sort_count >= MAX_PENDING_TXS) break;
        
        
        bool exists = false;
        for (size_t j = 0; j < cluster->tx_sort_count; j++) {
            if (cluster->tx_sort_map[j].tx_id == sort_info[i].tx_id) {
                
                cluster->tx_sort_map[j] = sort_info[i];
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            
            cluster->tx_sort_map[cluster->tx_sort_count++] = sort_info[i];
        }
    }
    
    return 0;
}

int tee_cluster_get_tx_sort_order(tee_cluster_state_t* cluster,
                                    uint64_t tx_id,
                                    uint64_t* sort_order) {
    if (cluster == NULL || sort_order == NULL) return -1;
    
    
    for (size_t i = 0; i < cluster->tx_sort_count; i++) {
        if (cluster->tx_sort_map[i].tx_id == tx_id) {
            *sort_order = cluster->tx_sort_map[i].sort_order;
            return 0;
        }
    }
    
    
    return -1;
}

int tee_cluster_add_executed_tx(tee_cluster_state_t* cluster,
                                 uint64_t tx_id,
                                 uint32_t chain_id,
                                 uint64_t block_number,
                                 uint64_t log_index) {
    if (cluster == NULL) return -1;
    
    if (cluster->executed_count >= MAX_PENDING_TXS) return -1;
    
    
    for (size_t i = 0; i < cluster->executed_count; i++) {
        if (cluster->executed_txs[i].tx_id == tx_id &&
            cluster->executed_txs[i].chain_id == chain_id) {
            
            cluster->executed_txs[i].block_number = block_number;
            cluster->executed_txs[i].log_index = log_index;
            cluster->executed_txs[i].has_log = true;
            return 0;
        }
    }
    
    
    executed_tx_t* tx = &cluster->executed_txs[cluster->executed_count++];
    tx->tx_id = tx_id;
    tx->chain_id = chain_id;
    tx->block_number = block_number;
    tx->log_index = log_index;
    tx->has_log = true;
    
    return 0;
}

int tee_cluster_leader_broadcast_tx_set(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    if (!raft_is_leader(&cluster->raft)) {
        return -1;
    }
    
    
    executed_tx_t tx_set[MAX_PENDING_TXS];
    size_t tx_set_count = 0;
    
    for (size_t i = 0; i < cluster->executed_count; i++) {
        if (cluster->executed_txs[i].has_log) {
            if (tx_set_count >= MAX_PENDING_TXS) break;
            tx_set[tx_set_count++] = cluster->executed_txs[i];
        }
    }
    
    if (tx_set_count == 0) {
        return 0; 
    }
    
    
    uint8_t broadcast_payload[sizeof(executed_tx_t) * MAX_PENDING_TXS + 16];
    size_t offset = 0;
    
    
    memcpy(broadcast_payload + offset, &cluster->current_epoch, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    memcpy(broadcast_payload + offset, &tx_set_count, sizeof(size_t));
    offset += sizeof(size_t);
    
    
    memcpy(broadcast_payload + offset, tx_set, tx_set_count * sizeof(executed_tx_t));
    offset += tx_set_count * sizeof(executed_tx_t);
    
    
    tee_network_broadcast(&cluster->network, MSG_TX_SET_BROADCAST,
                          broadcast_payload, offset);
    
    return 0;
}

int tee_cluster_receive_and_sign_tx_set(tee_cluster_state_t* cluster,
                                          const executed_tx_t* tx_set,
                                          size_t count,
                                          uint8_t* signature) {
    if (cluster == NULL || tx_set == NULL || signature == NULL) return -1;
    
    
    for (size_t i = 0; i < count; i++) {
        bool found = false;
        for (size_t j = 0; j < cluster->executed_count; j++) {
            if (cluster->executed_txs[j].tx_id == tx_set[i].tx_id &&
                cluster->executed_txs[j].chain_id == tx_set[i].chain_id &&
                cluster->executed_txs[j].has_log) {
                found = true;
                break;
            }
        }
        if (!found) {
            return -1; 
        }
    }
    
    
    raft_log_entry_t entry;
    memset(&entry, 0, sizeof(raft_log_entry_t));
    entry.term = cluster->raft.current_term;
    entry.index = cluster->raft.log_size;
    entry.tx_id = 0; 
    entry.timestamp = 0; 
    
    
    uint8_t tx_set_hash[32];
    platform_sha256((const uint8_t*)tx_set, count * sizeof(executed_tx_t), tx_set_hash);
    memcpy(entry.data, tx_set_hash, 32);
    entry.data_size = 32;
    
    
    if (raft_append_entry(&cluster->raft, &entry) != 0) {
        return -1;
    }
    
    
    uint8_t message[sizeof(executed_tx_t) * MAX_PENDING_TXS + 16];
    size_t msg_size = count * sizeof(executed_tx_t);
    memcpy(message, tx_set, msg_size);
    
    
    
    platform_sha256(message, msg_size, signature);
    memcpy(signature + 32, message, 32); 
    
    return 0;
}

int tee_cluster_listen_and_build_dag(tee_cluster_state_t* cluster,
                                      uint32_t chain_id,
                                      const operation_t* op) {
    if (cluster == NULL || op == NULL) return -1;
    
    
    uint64_t tx_sort_order = UINT64_MAX;
    if (tee_cluster_get_tx_sort_order(cluster, op->tx_id, &tx_sort_order) != 0) {
        
        tx_sort_order = op->tx_id;
    }
    
    
    merkle_crdt_dag_t* dag = get_global_dag(cluster);
    if (dag == NULL) return -1;
    
    
    int ret = merkle_crdt_add_operation(dag, op, tx_sort_order);
    if (ret != 0) return ret;
    
    
    dag_node_t* new_node = NULL;
    for (size_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i]->operation.operation_id == op->operation_id) {
            new_node = dag->nodes[i];
            break;
        }
    }
    
    if (new_node == NULL) return -1;
    
    
    mpt_tree_t* token_tree = NULL;
    for (size_t i = 0; i < cluster->token_count; i++) {
        if (memcmp(cluster->token_addresses[i], op->token_address, 42) == 0) {
            token_tree = &cluster->token_trees[i];
            break;
        }
    }
    
    if (token_tree == NULL) {
        
        if (cluster->token_count >= MAX_NODES) return -1;
        size_t idx = cluster->token_count;
        memcpy(cluster->token_addresses[idx], op->token_address, 42);
        mpt_tree_init(&cluster->token_trees[idx]);
        token_tree = &cluster->token_trees[idx];
        cluster->token_count++;
    }
    
    
    merkle_crdt_update_parent_states(dag, new_node, token_tree);
    
    
    operation_t tx_operations[100];
    size_t tx_op_count = 0;
    
    if (merkle_crdt_collect_tx_operations(dag, op->tx_id, tx_operations, &tx_op_count, 100) == 0) {
        
        if (tx_op_count >= 2) {
            
            bool tx_failed = !merkle_crdt_validate_tx(tx_operations, tx_op_count, token_tree);
            
            
            for (size_t i = 0; i < dag->node_count; i++) {
                if (dag->nodes[i]->operation.tx_id == op->tx_id) {
                    
                    bool node_failed = merkle_crdt_check_operation_failed(dag, dag->nodes[i], token_tree);
                    dag->nodes[i]->is_failed = tx_failed || node_failed;
                    
                    if (dag->nodes[i]->is_failed) {
                        dag->nodes[i]->operation.is_valid = false;
                    }
                }
            }
            
            if (tx_failed) {
                return -1; 
            }
        }
    }
    
    return 0;
}

int tee_cluster_generate_and_send_epoch_output(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    uint8_t dag_head[32];
    uint8_t state_root[32];
    uint8_t reject_root[32];
    
    if (tee_cluster_generate_epoch_output(cluster, state_root, dag_head, reject_root) != 0) {
        return -1;
    }
    
    
    epoch_output_t output;
    memset(&output, 0, sizeof(epoch_output_t));
    output.epoch_id = cluster->current_epoch;
    output.node_id = cluster->my_node_id;
    memcpy(output.dag_head, dag_head, 32);
    memcpy(output.state_root, state_root, 32);
    memcpy(output.reject_root, reject_root, 32);
    
    
    uint8_t message[sizeof(epoch_output_t) - 64]; 
    size_t msg_size = sizeof(epoch_output_t) - 64;
    memcpy(message, &output.epoch_id, sizeof(uint64_t));
    memcpy(message + sizeof(uint64_t), &output.node_id, sizeof(uint32_t));
    memcpy(message + sizeof(uint64_t) + sizeof(uint32_t), output.dag_head, 32);
    memcpy(message + sizeof(uint64_t) + sizeof(uint32_t) + 32, output.state_root, 32);
    memcpy(message + sizeof(uint64_t) + sizeof(uint32_t) + 64, output.reject_root, 32);
    
    
    platform_sha256(message, msg_size, output.signature);
    memcpy(output.signature + 32, message, 32); 
    
    
    if (cluster->current_leader != 0) {
        uint8_t payload[sizeof(epoch_output_t)];
        memcpy(payload, &output, sizeof(epoch_output_t));
        
        tee_network_send_message(&cluster->network, cluster->current_leader,
                                  MSG_EPOCH_OUTPUT, payload, sizeof(epoch_output_t));
    }
    
    return 0;
}

int tee_cluster_leader_collect_epoch_outputs(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    if (!raft_is_leader(&cluster->raft)) {
        return -1;
    }
    
    
    cluster->epoch_output_count = 0;
    cluster->epoch_output_collected = false;
    
    
    tee_message_t message;
    while (tee_network_receive_message(&cluster->network, &message) == 0) {
        if (message.header.type == MSG_EPOCH_OUTPUT) {
            if (cluster->epoch_output_count >= MAX_CLUSTER_NODES) break;
            
            epoch_output_t* output = (epoch_output_t*)message.payload;
            
            
            
            
            
            cluster->epoch_outputs[cluster->epoch_output_count++] = *output;
        }
    }
    
    
    size_t majority_count = (cluster->node_count / 2) + 1;
    if (cluster->epoch_output_count < majority_count) {
        return -1; 
    }
    
    
    if (cluster->epoch_output_count > 0) {
        epoch_output_t* first = &cluster->epoch_outputs[0];
        size_t match_count = 1;
        
        for (size_t i = 1; i < cluster->epoch_output_count; i++) {
            epoch_output_t* current = &cluster->epoch_outputs[i];
            
            if (memcmp(first->dag_head, current->dag_head, 32) == 0 &&
                memcmp(first->state_root, current->state_root, 32) == 0 &&
                memcmp(first->reject_root, current->reject_root, 32) == 0) {
                match_count++;
            }
        }
        
        if (match_count >= majority_count) {
            cluster->epoch_output_collected = true;
            return 0; 
        }
    }
    
    return -1; 
}

int tee_cluster_leader_sync_to_l2_chains(tee_cluster_state_t* cluster) {
    if (cluster == NULL) return -1;
    
    
    if (!raft_is_leader(&cluster->raft)) {
        return -1;
    }
    
    
    if (!cluster->epoch_output_collected) {
        return -1;
    }
    
    if (cluster->epoch_output_count == 0) {
        return -1;
    }
    
    
    epoch_output_t* output = &cluster->epoch_outputs[0];
    
    
    uint8_t sync_payload[sizeof(epoch_output_t) + 16];
    size_t offset = 0;
    
    memcpy(sync_payload + offset, &output->epoch_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(sync_payload + offset, output->dag_head, 32);
    offset += 32;
    memcpy(sync_payload + offset, output->state_root, 32);
    offset += 32;
    memcpy(sync_payload + offset, output->reject_root, 32);
    offset += 32;
    
    
    
    tee_network_broadcast(&cluster->network, MSG_EPOCH_SYNC_TO_L2,
                          sync_payload, offset);
    
    return 0;
}

int tee_cluster_sync_node_from_other_tee(tee_cluster_state_t* local_cluster,
                                         const dag_node_t* remote_node) {
    if (local_cluster == NULL || remote_node == NULL) return -1;
    
    merkle_crdt_dag_t* local_dag = get_global_dag(local_cluster);
    if (local_dag == NULL) return -1;
    
    
    for (size_t i = 0; i < local_dag->node_count; i++) {
        if (local_dag->nodes[i]->node_id == remote_node->node_id) {
            return 0; 
        }
    }
    
    
    bool all_parents_exist = true;
    for (size_t i = 0; i < remote_node->parent_count; i++) {
        bool parent_exists = false;
        for (size_t j = 0; j < local_dag->node_count; j++) {
            if (local_dag->nodes[j]->node_id == remote_node->parents[i]->node_id) {
                parent_exists = true;
                break;
            }
        }
        if (!parent_exists) {
            all_parents_exist = false;
            break;
        }
    }
    
    
    if (!all_parents_exist) {
        return -1;
    }
    
    
    
    uint64_t tx_sort_order = remote_node->tx_sort_order;
    
    
    int ret = merkle_crdt_add_operation(local_dag, &remote_node->operation, tx_sort_order);
    if (ret != 0) return ret;
    
    
    dag_node_t* new_node = NULL;
    for (size_t i = 0; i < local_dag->node_count; i++) {
        if (local_dag->nodes[i]->node_id == remote_node->node_id) {
            new_node = local_dag->nodes[i];
            break;
        }
    }
    
    if (new_node == NULL) return -1;
    
    
    new_node->is_head_candidate = remote_node->is_head_candidate;
    new_node->is_processed = remote_node->is_processed;
    new_node->state_updated = remote_node->state_updated;
    
    return 0;
}

static void compute_in_degrees(const merkle_crdt_dag_t* dag, 
                                std::map<uint64_t, size_t>& in_degree_map,
                                std::set<uint64_t>& node_id_set) {
    
    for (size_t i = 0; i < dag->node_count; i++) {
        uint64_t node_id = dag->nodes[i]->node_id;
        node_id_set.insert(node_id);
        in_degree_map[node_id] = 0;
    }
    
    
    for (size_t i = 0; i < dag->node_count; i++) {
        const dag_node_t* node = dag->nodes[i];
        for (size_t j = 0; j < node->parent_count; j++) {
            uint64_t parent_id = node->parents[j]->node_id;
            if (in_degree_map.find(parent_id) != in_degree_map.end()) {
                in_degree_map[parent_id]++;
            }
        }
    }
}

int tee_cluster_sync_all_tee_dags(tee_cluster_state_t* local_cluster,
                                   const tee_cluster_state_t* remote_cluster) {
    if (local_cluster == NULL || remote_cluster == NULL) return -1;
    
    merkle_crdt_dag_t* local_dag = get_global_dag(local_cluster);
    const merkle_crdt_dag_t* remote_dag = &remote_cluster->global_dag;
    
    if (local_dag == NULL || remote_dag == NULL) return -1;
    
    
    uint8_t local_root[32], remote_root[32];
    
    if (merkle_crdt_compute_dag_root_hash(local_dag, local_root) == 0) {
        
        merkle_crdt_dag_t* temp_remote_dag = (merkle_crdt_dag_t*)remote_dag;
        if (merkle_crdt_compute_dag_root_hash(temp_remote_dag, remote_root) == 0) {
            if (memcmp(local_root, remote_root, 32) == 0) {
                
                return 0;
            }
        }
    }
    
    
    std::set<uint64_t> local_node_ids;
    for (size_t i = 0; i < local_dag->node_count; i++) {
        local_node_ids.insert(local_dag->nodes[i]->node_id);
    }
    
    
    
    std::map<uint64_t, size_t> in_degree_map;
    std::set<uint64_t> remote_node_id_set;
    compute_in_degrees(remote_dag, in_degree_map, remote_node_id_set);
    
    
    bool changed = true;
    size_t max_iterations = remote_dag->node_count;  
    size_t iteration = 0;
    
    while (changed && iteration < max_iterations) {
        changed = false;
        iteration++;
        
        
        for (size_t i = 0; i < remote_dag->node_count; i++) {
            const dag_node_t* remote_node = remote_dag->nodes[i];
            uint64_t node_id = remote_node->node_id;
            
            
            if (local_node_ids.find(node_id) != local_node_ids.end()) {
                continue;  
            }
            
            
            bool all_parents_exist = true;
            for (size_t p = 0; p < remote_node->parent_count; p++) {
                uint64_t parent_id = remote_node->parents[p]->node_id;
                if (local_node_ids.find(parent_id) == local_node_ids.end()) {
                    all_parents_exist = false;
                    break;
                }
            }
            
            
            if (all_parents_exist) {
                if (tee_cluster_sync_node_from_other_tee(local_cluster, remote_node) == 0) {
                    local_node_ids.insert(node_id);  
                    changed = true;
                }
            }
        }
    }
    
    return 0;
}
