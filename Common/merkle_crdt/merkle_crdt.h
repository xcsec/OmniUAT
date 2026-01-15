#ifndef _MERKLE_CRDT_H_
#define _MERKLE_CRDT_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "mpt_tree.h"
#include <vector>

#define MAX_OPERATION_DATA_LEN 256
#define MAX_DAG_NODES 100000
#define MAX_PARENTS 16
#define MAX_CHILDREN 32
#define MAX_REJECT_NODES 10000

typedef enum {
    OP_ADD = 0,      
    OP_SUBTRACT = 1, 
    OP_SET = 2       
} operation_type_t;

typedef struct {
    uint64_t operation_id;
    uint64_t tx_id;              
    uint64_t timestamp;
    operation_type_t type;
    uint8_t token_address[42];   
    uint8_t account[20];         
    uint8_t amount[32];          
    uint8_t hash[32];            
    bool is_valid;               
} operation_t;

typedef struct dag_node {
    uint64_t node_id;
    operation_t operation;
    uint64_t tx_sort_order;  
    
    
    struct dag_node* parents[MAX_PARENTS];
    size_t parent_count;
    
    
    struct dag_node* children[MAX_CHILDREN];
    size_t child_count;
    
    
    struct dag_node* neighbors[MAX_CHILDREN];
    size_t neighbor_count;
    
    
    uint8_t merkle_hash[32];     
    bool is_head_candidate;      
    bool is_processed;            
    
    
    bool state_updated;           
    
    
    bool is_failed;               
} dag_node_t;

#define CONFLICT_INDEX_SIZE 1024  
typedef struct conflict_index_entry {
    dag_node_t* node;
    struct conflict_index_entry* next;
} conflict_index_entry_t;

typedef struct {
    dag_node_t* nodes[MAX_DAG_NODES];
    size_t node_count;
    
    
    dag_node_t* head;
    uint8_t head_hash[32];
    
    
    dag_node_t* latest_nodes[MAX_DAG_NODES];
    size_t latest_count;
    
    
    uint64_t node_id_map[MAX_DAG_NODES];
    
    
    
    conflict_index_entry_t* conflict_index[CONFLICT_INDEX_SIZE];
} merkle_crdt_dag_t;

int merkle_crdt_init(merkle_crdt_dag_t* dag);

int merkle_crdt_add_operation(merkle_crdt_dag_t* dag, const operation_t* op, uint64_t tx_sort_order);

bool merkle_crdt_is_conflict(const operation_t* op1, const operation_t* op2);

int merkle_crdt_connect_nodes(merkle_crdt_dag_t* dag, 
                               dag_node_t* child, 
                               dag_node_t* parent);

int merkle_crdt_connect_neighbors(merkle_crdt_dag_t* dag,
                                   dag_node_t* node1,
                                   dag_node_t* node2);

int merkle_crdt_generate_head(merkle_crdt_dag_t* dag);

int merkle_crdt_update_state(merkle_crdt_dag_t* dag, mpt_tree_t* token_tree);

void merkle_crdt_node_hash(dag_node_t* node, uint8_t* hash);

int merkle_crdt_compute_dag_root_hash(merkle_crdt_dag_t* dag, uint8_t* root_hash);

bool merkle_crdt_check_operation_failed(merkle_crdt_dag_t* dag, 
                                        dag_node_t* node,
                                        mpt_tree_t* token_tree);

bool merkle_crdt_validate_tx(const operation_t* operations, size_t op_count,
                              mpt_tree_t* token_tree);

int merkle_crdt_update_parent_states(merkle_crdt_dag_t* dag, 
                                      dag_node_t* new_node,
                                      mpt_tree_t* token_tree);

int merkle_crdt_collect_tx_operations(merkle_crdt_dag_t* dag, 
                                       uint64_t tx_id,
                                       operation_t* operations,
                                       size_t* op_count,
                                       size_t max_ops);

int merkle_crdt_find_block_related_nodes(merkle_crdt_dag_t* dag,
                                         const std::vector<uint64_t>& block_tx_ids,
                                         dag_node_t** nodes,
                                         size_t* node_count,
                                         size_t max_nodes);

int merkle_crdt_create_reverse_operation(const operation_t* original_op,
                                          operation_t* reverse_op);

#endif
