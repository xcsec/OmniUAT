#include "merkle_crdt.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

static void operation_hash(const operation_t* op, uint8_t* hash) {
    if (op == NULL || hash == NULL) return;
    
    uint8_t buffer[512];
    size_t offset = 0;
    
    memcpy(buffer + offset, &op->operation_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &op->tx_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &op->type, sizeof(operation_type_t));
    offset += sizeof(operation_type_t);
    memcpy(buffer + offset, op->token_address, 42);
    offset += 42;
    memcpy(buffer + offset, op->account, 20);
    offset += 20;
    memcpy(buffer + offset, op->amount, 32);
    offset += 32;
    
    platform_sha256(buffer, offset, (uint8_t*)hash);
}

static uint32_t conflict_index_hash(const uint8_t* account, const uint8_t* token) {
    if (account == NULL || token == NULL) return 0;
    
    
    uint32_t hash = 0;
    for (int i = 0; i < 4 && i < 20; i++) {
        hash ^= ((uint32_t)account[i]) << (i * 8);
    }
    for (int i = 0; i < 4 && i < 42; i++) {
        hash ^= ((uint32_t)token[i]) << ((i % 4) * 8);
    }
    return hash % CONFLICT_INDEX_SIZE;
}

int merkle_crdt_init(merkle_crdt_dag_t* dag) {
    if (dag == NULL) return -1;
    
    memset(dag, 0, sizeof(merkle_crdt_dag_t));
    dag->head = NULL;
    
    return 0;
}

static dag_node_t* dag_node_create(const operation_t* op) {
    if (op == NULL) return NULL;
    
    dag_node_t* node = (dag_node_t*)platform_malloc(sizeof(dag_node_t));
    if (node == NULL) return NULL;
    
    memset(node, 0, sizeof(dag_node_t));
    memcpy(&node->operation, op, sizeof(operation_t));
    node->node_id = op->operation_id;
    node->tx_sort_order = 0;  
    node->is_failed = false;  
    
    
    operation_hash(op, node->operation.hash);
    
    
    merkle_crdt_node_hash(node, node->merkle_hash);
    
    return node;
}

bool merkle_crdt_is_conflict(const operation_t* op1, const operation_t* op2) {
    if (op1 == NULL || op2 == NULL) return false;
    
    
    if (memcmp(op1->token_address, op2->token_address, 42) != 0) {
        return false;
    }
    if (memcmp(op1->account, op2->account, 20) != 0) {
        return false;
    }
    
    
    
    if ((op1->type == OP_ADD && op2->type == OP_SUBTRACT) ||
        (op1->type == OP_SUBTRACT && op2->type == OP_ADD) ||
        (op1->type == OP_SUBTRACT && op2->type == OP_SUBTRACT)) {
        return true;
    }
    
    return false;
}

int merkle_crdt_add_operation(merkle_crdt_dag_t* dag, const operation_t* op, uint64_t tx_sort_order) {
    if (dag == NULL || op == NULL) return -1;
    
    if (dag->node_count >= MAX_DAG_NODES) return -1;
    
    
    dag_node_t* new_node = dag_node_create(op);
    if (new_node == NULL) return -1;
    
    
    new_node->tx_sort_order = tx_sort_order;
    
    
    dag->nodes[dag->node_count] = new_node;
    dag->node_id_map[dag->node_count] = new_node->node_id;
    dag->node_count++;
    
    
    if (dag->latest_count < MAX_DAG_NODES) {
        dag->latest_nodes[dag->latest_count++] = new_node;
    }
    
    
    uint32_t hash = conflict_index_hash(op->account, op->token_address);
    
    
    conflict_index_entry_t* entry = dag->conflict_index[hash];
    
    while (entry != NULL) {
        dag_node_t* existing_node = entry->node;
        
        if (merkle_crdt_is_conflict(&new_node->operation, &existing_node->operation)) {
            
            if (new_node->tx_sort_order > existing_node->tx_sort_order) {
                
                merkle_crdt_connect_nodes(dag, new_node, existing_node);
            } else {
                
                merkle_crdt_connect_nodes(dag, existing_node, new_node);
            }
        } else {
            
            merkle_crdt_connect_neighbors(dag, new_node, existing_node);
        }
        
        entry = entry->next;
    }
    
    
    
    if (op->type == OP_SUBTRACT || op->type == OP_ADD) {
        conflict_index_entry_t* new_entry = (conflict_index_entry_t*)platform_malloc(sizeof(conflict_index_entry_t));
        if (new_entry != NULL) {
            new_entry->node = new_node;
            new_entry->next = dag->conflict_index[hash];
            dag->conflict_index[hash] = new_entry;
        }
    }
    
    
    
    if (dag->node_count > 100) {
        
        size_t neighbor_count = (dag->latest_count < 20) ? dag->latest_count : 20;
        for (size_t i = 0; i < neighbor_count && i < dag->latest_count - 1; i++) {
            dag_node_t* existing_node = dag->latest_nodes[i];
            if (existing_node != new_node && 
                !merkle_crdt_is_conflict(&new_node->operation, &existing_node->operation)) {
                merkle_crdt_connect_neighbors(dag, new_node, existing_node);
            }
        }
    } else {
        
        for (size_t i = 0; i < dag->node_count - 1; i++) {
            dag_node_t* existing_node = dag->nodes[i];
            if (!merkle_crdt_is_conflict(&new_node->operation, &existing_node->operation)) {
                merkle_crdt_connect_neighbors(dag, new_node, existing_node);
            }
        }
    }
    
    return 0;
}

int merkle_crdt_collect_tx_operations(merkle_crdt_dag_t* dag, 
                                       uint64_t tx_id,
                                       operation_t* operations,
                                       size_t* op_count,
                                       size_t max_ops) {
    if (dag == NULL || operations == NULL || op_count == NULL) return -1;
    
    *op_count = 0;
    
    
    for (size_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i]->operation.tx_id == tx_id) {
            if (*op_count >= max_ops) return -1;
            memcpy(&operations[*op_count], &dag->nodes[i]->operation, sizeof(operation_t));
            (*op_count)++;
        }
    }
    
    return 0;
}

int merkle_crdt_connect_nodes(merkle_crdt_dag_t* dag,
                               dag_node_t* child,
                               dag_node_t* parent) {
    if (dag == NULL || child == NULL || parent == NULL) return -1;
    
    
    for (size_t i = 0; i < child->parent_count; i++) {
        if (child->parents[i] == parent) return 0;
    }
    
    if (child->parent_count >= MAX_PARENTS) return -1;
    if (parent->child_count >= MAX_CHILDREN) return -1;
    
    
    child->parents[child->parent_count++] = parent;
    parent->children[parent->child_count++] = child;
    
    
    merkle_crdt_node_hash(child, child->merkle_hash);
    merkle_crdt_node_hash(parent, parent->merkle_hash);
    
    return 0;
}

int merkle_crdt_update_parent_states(merkle_crdt_dag_t* dag, 
                                      dag_node_t* new_node,
                                      mpt_tree_t* token_tree) {
    if (dag == NULL || new_node == NULL || token_tree == NULL) return -1;
    
    
    for (size_t i = 0; i < new_node->parent_count; i++) {
        dag_node_t* parent = new_node->parents[i];
        
        
        if (!parent->state_updated) {
            
            uint8_t key[64];
            memcpy(key, parent->operation.account, 20);
            memcpy(key + 20, parent->operation.token_address, 42);
            
            
            uint8_t current_balance[32];
            size_t balance_len = 32;
            int ret = mpt_tree_get(token_tree, key, 64, current_balance, &balance_len);
            
            uint8_t new_balance[32];
            memset(new_balance, 0, 32);
            
            if (ret == 0 && balance_len == 32) {
                
                memcpy(new_balance, current_balance, 32);
                
                
                if (parent->operation.type == OP_ADD) {
                    
                    bool carry = false;
                    for (int j = 31; j >= 0; j--) {
                        uint16_t sum = (uint16_t)new_balance[j] + 
                                       (uint16_t)parent->operation.amount[j] + 
                                       (carry ? 1 : 0);
                        new_balance[j] = (uint8_t)(sum & 0xFF);
                        carry = (sum > 0xFF);
                    }
                } else if (parent->operation.type == OP_SUBTRACT) {
                    
                    bool borrow = false;
                    for (int j = 31; j >= 0; j--) {
                        int16_t diff = (int16_t)new_balance[j] - 
                                       (int16_t)parent->operation.amount[j] - 
                                       (borrow ? 1 : 0);
                        if (diff < 0) {
                            diff += 256;
                            borrow = true;
                        } else {
                            borrow = false;
                        }
                        new_balance[j] = (uint8_t)(diff & 0xFF);
                    }
                } else if (parent->operation.type == OP_SET) {
                    
                    memcpy(new_balance, parent->operation.amount, 32);
                }
            } else {
                
                if (parent->operation.type == OP_ADD || parent->operation.type == OP_SET) {
                    memcpy(new_balance, parent->operation.amount, 32);
                }
                
            }
            
            
            mpt_tree_insert(token_tree, key, 64, new_balance, 32);
            parent->state_updated = true;
        }
    }
    
    return 0;
}

int merkle_crdt_connect_neighbors(merkle_crdt_dag_t* dag,
                                   dag_node_t* node1,
                                   dag_node_t* node2) {
    if (dag == NULL || node1 == NULL || node2 == NULL) return -1;
    
    
    for (size_t i = 0; i < node1->neighbor_count; i++) {
        if (node1->neighbors[i] == node2) return 0;
    }
    
    if (node1->neighbor_count >= MAX_CHILDREN) return -1;
    if (node2->neighbor_count >= MAX_CHILDREN) return -1;
    
    
    node1->neighbors[node1->neighbor_count++] = node2;
    node2->neighbors[node2->neighbor_count++] = node1;
    
    return 0;
}

void merkle_crdt_node_hash(dag_node_t* node, uint8_t* hash) {
    if (node == NULL || hash == NULL) return;
    
    uint8_t buffer[2048];
    size_t offset = 0;
    
    
    memcpy(buffer + offset, node->operation.hash, 32);
    offset += 32;
    
    
    for (size_t i = 0; i < node->parent_count; i++) {
        memcpy(buffer + offset, node->parents[i]->merkle_hash, 32);
        offset += 32;
    }
    
    
    for (size_t i = 0; i < node->child_count; i++) {
        memcpy(buffer + offset, node->children[i]->merkle_hash, 32);
        offset += 32;
    }
    
    platform_sha256(buffer, offset, hash);
}

int merkle_crdt_generate_head(merkle_crdt_dag_t* dag) {
    if (dag == NULL) return -1;
    
    
    if (dag->head == NULL) {
        dag->head = (dag_node_t*)platform_malloc(sizeof(dag_node_t));
        if (dag->head == NULL) return -1;
        memset(dag->head, 0, sizeof(dag_node_t));
        dag->head->node_id = UINT64_MAX; 
    }
    
    
    dag->head->child_count = 0;
    
    
    for (size_t i = 0; i < dag->node_count; i++) {
        dag_node_t* node = dag->nodes[i];
        if (node->child_count == 0 && !node->is_processed) {
            
            if (dag->head->child_count < MAX_CHILDREN) {
                dag->head->children[dag->head->child_count++] = node;
                node->is_head_candidate = true;
            }
        }
    }
    
    
    merkle_crdt_node_hash(dag->head, dag->head_hash);
    
    return 0;
}

int merkle_crdt_compute_dag_root_hash(merkle_crdt_dag_t* dag, uint8_t* root_hash) {
    if (dag == NULL || root_hash == NULL) return -1;
    
    
    if (dag->node_count == 0) {
        memset(root_hash, 0, 32);
        return 0;
    }
    
    
    if (merkle_crdt_generate_head(dag) != 0) {
        return -1;
    }
    
    
    
    memcpy(root_hash, dag->head_hash, 32);
    
    return 0;
}

int merkle_crdt_update_state(merkle_crdt_dag_t* dag, mpt_tree_t* token_tree) {
    if (dag == NULL || token_tree == NULL) return -1;
    
    if (dag->head == NULL) return -1;
    
    
    for (size_t i = 0; i < dag->head->child_count; i++) {
        dag_node_t* node = dag->head->children[i];
        
        if (node->state_updated) continue;
        
        
        uint8_t key[64];
        memcpy(key, node->operation.account, 20);
        memcpy(key + 20, node->operation.token_address, 42);
        
        
        uint8_t current_balance[32];
        size_t balance_len = 32;
        int ret = mpt_tree_get(token_tree, key, 64, current_balance, &balance_len);
        
        uint8_t new_balance[32];
        memset(new_balance, 0, 32);
        
        if (ret == 0 && balance_len == 32) {
            
            memcpy(new_balance, current_balance, 32);
            
            
            if (node->operation.type == OP_ADD) {
                
                bool carry = false;
                for (int j = 31; j >= 0; j--) {
                    uint16_t sum = (uint16_t)new_balance[j] + 
                                   (uint16_t)node->operation.amount[j] + 
                                   (carry ? 1 : 0);
                    new_balance[j] = (uint8_t)(sum & 0xFF);
                    carry = (sum > 0xFF);
                }
            } else if (node->operation.type == OP_SUBTRACT) {
                
                bool borrow = false;
                for (int j = 31; j >= 0; j--) {
                    int16_t diff = (int16_t)new_balance[j] - 
                                   (int16_t)node->operation.amount[j] - 
                                   (borrow ? 1 : 0);
                    if (diff < 0) {
                        diff += 256;
                        borrow = true;
                    } else {
                        borrow = false;
                    }
                    new_balance[j] = (uint8_t)(diff & 0xFF);
                }
            } else if (node->operation.type == OP_SET) {
                
                memcpy(new_balance, node->operation.amount, 32);
            }
        } else {
            
            if (node->operation.type == OP_ADD || node->operation.type == OP_SET) {
                memcpy(new_balance, node->operation.amount, 32);
            }
            
        }
        
        
        mpt_tree_insert(token_tree, key, 64, new_balance, 32);
        node->state_updated = true;
        node->is_processed = true;
    }
    
    return 0;
}

bool merkle_crdt_validate_tx(const operation_t* operations, size_t op_count,
                              mpt_tree_t* token_tree) {
    if (operations == NULL || token_tree == NULL || op_count == 0) return false;
    
    
    
    struct {
        uint8_t key[64];
        uint8_t balance[32];
        bool exists;
    } temp_balances[100];
    size_t temp_count = 0;
    
    
    for (size_t i = 0; i < op_count; i++) {
        const operation_t* op = &operations[i];
        
        uint8_t key[64];
        memcpy(key, op->account, 20);
        memcpy(key + 20, op->token_address, 42);
        
        
        bool found = false;
        for (size_t j = 0; j < temp_count; j++) {
            if (memcmp(temp_balances[j].key, key, 64) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found && temp_count < 100) {
            memcpy(temp_balances[temp_count].key, key, 64);
            size_t balance_len = 32;
            if (mpt_tree_get(token_tree, key, 64, temp_balances[temp_count].balance, &balance_len) == 0) {
                temp_balances[temp_count].exists = true;
            } else {
                memset(temp_balances[temp_count].balance, 0, 32);
                temp_balances[temp_count].exists = false;
            }
            temp_count++;
        }
    }
    
    
    for (size_t i = 0; i < op_count; i++) {
        const operation_t* op = &operations[i];
        
        uint8_t key[64];
        memcpy(key, op->account, 20);
        memcpy(key + 20, op->token_address, 42);
        
        
        uint8_t* balance = NULL;
        bool account_exists = false;
        for (size_t j = 0; j < temp_count; j++) {
            if (memcmp(temp_balances[j].key, key, 64) == 0) {
                balance = temp_balances[j].balance;
                account_exists = temp_balances[j].exists;
                break;
            }
        }
        
        if (balance == NULL) continue;
        
        
        if (op->type == OP_ADD) {
            
            bool carry = false;
            for (int j = 31; j >= 0; j--) {
                uint16_t sum = (uint16_t)balance[j] + 
                               (uint16_t)op->amount[j] + 
                               (carry ? 1 : 0);
                balance[j] = (uint8_t)(sum & 0xFF);
                carry = (sum > 0xFF);
            }
        } else if (op->type == OP_SUBTRACT) {
            
            if (!account_exists) {
                
                return false;
            }
            
            bool borrow = false;
            for (int j = 31; j >= 0; j--) {
                int16_t diff = (int16_t)balance[j] - 
                               (int16_t)op->amount[j] - 
                               (borrow ? 1 : 0);
                if (diff < 0) {
                    diff += 256;
                    borrow = true;
                } else {
                    borrow = false;
                }
                balance[j] = (uint8_t)(diff & 0xFF);
            }
            
            
            if (borrow) {
                
                return false;
            }
            
            
            bool is_zero = true;
            for (int j = 0; j < 32; j++) {
                if (balance[j] != 0) {
                    is_zero = false;
                    break;
                }
            }
            
            if (is_zero) {
                return false;  
            }
        } else if (op->type == OP_SET) {
            
            memcpy(balance, op->amount, 32);
        }
        
        
        
        
    }
    
    return true;
}

bool merkle_crdt_check_operation_failed(merkle_crdt_dag_t* dag, 
                                        dag_node_t* node,
                                        mpt_tree_t* token_tree) {
    if (dag == NULL || node == NULL || token_tree == NULL) return false;
    
    
    operation_t tx_operations[100];
    size_t tx_op_count = 0;
    
    if (merkle_crdt_collect_tx_operations(dag, node->operation.tx_id, 
                                          tx_operations, &tx_op_count, 100) != 0) {
        return false;  
    }
    
    if (tx_op_count == 0) {
        return false;  
    }
    
    
    if (!merkle_crdt_validate_tx(tx_operations, tx_op_count, token_tree)) {
        return true;  
    }
    
    
    
    struct {
        uint8_t key[64];
        uint8_t balance[32];
        bool exists;
    } temp_balances[100];
    size_t temp_count = 0;
    
    
    for (size_t i = 0; i < tx_op_count; i++) {
        const operation_t* op = &tx_operations[i];
        
        uint8_t key[64];
        memcpy(key, op->account, 20);
        memcpy(key + 20, op->token_address, 42);
        
        
        bool found = false;
        for (size_t j = 0; j < temp_count; j++) {
            if (memcmp(temp_balances[j].key, key, 64) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found && temp_count < 100) {
            memcpy(temp_balances[temp_count].key, key, 64);
            size_t balance_len = 32;
            if (mpt_tree_get(token_tree, key, 64, temp_balances[temp_count].balance, &balance_len) == 0) {
                temp_balances[temp_count].exists = true;
            } else {
                memset(temp_balances[temp_count].balance, 0, 32);
                temp_balances[temp_count].exists = false;
            }
            temp_count++;
        }
    }
    
    
    for (size_t i = 0; i < tx_op_count; i++) {
        const operation_t* op = &tx_operations[i];
        
        uint8_t key[64];
        memcpy(key, op->account, 20);
        memcpy(key + 20, op->token_address, 42);
        
        
        uint8_t* balance = NULL;
        bool account_exists = false;
        for (size_t j = 0; j < temp_count; j++) {
            if (memcmp(temp_balances[j].key, key, 64) == 0) {
                balance = temp_balances[j].balance;
                account_exists = temp_balances[j].exists;
                break;
            }
        }
        
        if (balance == NULL) continue;
        
        
        if (op->type == OP_ADD) {
            
            bool carry = false;
            for (int j = 31; j >= 0; j--) {
                uint16_t sum = (uint16_t)balance[j] + 
                               (uint16_t)op->amount[j] + 
                               (carry ? 1 : 0);
                balance[j] = (uint8_t)(sum & 0xFF);
                carry = (sum > 0xFF);
            }
        } else if (op->type == OP_SUBTRACT) {
            
            if (!account_exists) {
                
                return true;
            }
            
            bool borrow = false;
            for (int j = 31; j >= 0; j--) {
                int16_t diff = (int16_t)balance[j] - 
                               (int16_t)op->amount[j] - 
                               (borrow ? 1 : 0);
                if (diff < 0) {
                    diff += 256;
                    borrow = true;
                } else {
                    borrow = false;
                }
                balance[j] = (uint8_t)(diff & 0xFF);
            }
            
            if (borrow) {
                
                return true;
            }
        } else if (op->type == OP_SET) {
            
            memcpy(balance, op->amount, 32);
        }
        
        
        bool is_zero = true;
        for (int j = 0; j < 32; j++) {
            if (balance[j] != 0) {
                is_zero = false;
                break;
            }
        }
        
        
        if (is_zero) {
            return true;  
        }
    }
    
    return false;  
}

int merkle_crdt_find_block_related_nodes(merkle_crdt_dag_t* dag,
                                         const std::vector<uint64_t>& block_tx_ids,
                                         dag_node_t** nodes,
                                         size_t* node_count,
                                         size_t max_nodes) {
    if (dag == NULL || nodes == NULL || node_count == NULL) return -1;
    
    *node_count = 0;
    
    
    if (block_tx_ids.empty()) {
        return 0;
    }
    
    
    std::unordered_set<uint64_t> visited_nodes;
    std::unordered_set<uint64_t> block_tx_set(block_tx_ids.begin(), block_tx_ids.end());
    
    
    visited_nodes.reserve(block_tx_ids.size() * 2);  
    
    
    
    for (size_t i = 0; i < dag->node_count && *node_count < max_nodes; i++) {
        dag_node_t* node = dag->nodes[i];
        
        
        if (block_tx_set.find(node->operation.tx_id) != block_tx_set.end()) {
            
            if (visited_nodes.find(node->node_id) == visited_nodes.end()) {
                nodes[*node_count] = node;
                (*node_count)++;
                visited_nodes.insert(node->node_id);
            }
        }
    }
    
    
    
    
    size_t start_idx = 0;
    while (start_idx < *node_count && *node_count < max_nodes) {
        dag_node_t* current_node = nodes[start_idx];
        start_idx++;
        
        
        
        
        size_t child_count = current_node->child_count;
        if (child_count > 0 && *node_count < max_nodes) {
            for (size_t i = 0; i < child_count && *node_count < max_nodes; i++) {
                dag_node_t* child = current_node->children[i];
                
                
                if (visited_nodes.find(child->node_id) == visited_nodes.end()) {
                    nodes[*node_count] = child;
                    (*node_count)++;
                    visited_nodes.insert(child->node_id);
                }
            }
        }
    }
    
    return 0;
}

int merkle_crdt_create_reverse_operation(const operation_t* original_op,
                                          operation_t* reverse_op) {
    if (original_op == NULL || reverse_op == NULL) return -1;
    
    
    memcpy(reverse_op, original_op, sizeof(operation_t));
    
    
    reverse_op->operation_id = original_op->operation_id + 0x8000000000000000ULL;  
    
    
    if (original_op->type == OP_ADD) {
        reverse_op->type = OP_SUBTRACT;  
    } else if (original_op->type == OP_SUBTRACT) {
        reverse_op->type = OP_ADD;  
    } else if (original_op->type == OP_SET) {
        
        reverse_op->is_valid = false;
        return -1;  
    }
    
    
    uint8_t buffer[512];
    size_t offset = 0;
    memcpy(buffer + offset, &reverse_op->operation_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &reverse_op->tx_id, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &reverse_op->type, sizeof(operation_type_t));
    offset += sizeof(operation_type_t);
    memcpy(buffer + offset, reverse_op->token_address, 42);
    offset += 42;
    memcpy(buffer + offset, reverse_op->account, 20);
    offset += 20;
    memcpy(buffer + offset, reverse_op->amount, 32);
    offset += 32;
    platform_sha256(buffer, offset, reverse_op->hash);
    
    return 0;
}
