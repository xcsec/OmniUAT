#ifndef _TEE_CLUSTER_H_
#define _TEE_CLUSTER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sequencer.h"
#include "merkle_crdt.h"
#include "../tee_network/tee_network.h"
#include "../raft/raft.h"

#define MAX_CLUSTER_NODES 16
#define MAX_PENDING_TXS 10000
#define LEADER_ELECTION_INTERVAL 10000  

typedef struct {
    uint32_t node_id;
    uint8_t public_key[64];
    uint8_t enclave_quote[2048];  
    bool is_active;
    bool is_leader;
    uint64_t last_heartbeat;
} tee_node_info_t;

typedef struct {
    uint64_t tx_id;
    uint64_t timestamp;
    uint8_t from[20];
    uint8_t to[20];
    uint8_t token_address[42];
    uint8_t amount[32];
    uint8_t signature[65];
    uint32_t chain_id;           
    bool is_processed;
} tx_request_t;

typedef struct {
    uint64_t tx_id;
    uint32_t chain_id;
    uint64_t block_number;       
    uint64_t log_index;          
    bool has_log;                
} executed_tx_t;

typedef struct {
    uint64_t tx_id;
    uint64_t sort_order;         
    uint64_t sort_timestamp;      
} tx_sort_info_t;

typedef struct {
    uint64_t epoch_id;
    uint32_t node_id;
    uint8_t dag_head[32];
    uint8_t state_root[32];
    uint8_t reject_root[32];
    uint8_t signature[64];        
} epoch_output_t;

typedef struct {
    
    tee_node_info_t nodes[MAX_CLUSTER_NODES];
    size_t node_count;
    uint32_t my_node_id;
    uint32_t current_leader;
    uint64_t last_leader_election;
    
    
    tx_request_t pending_txs[MAX_PENDING_TXS];
    size_t pending_count;
    
    
    tx_request_t sorted_txs[MAX_PENDING_TXS];
    size_t sorted_count;
    
    
    tx_sort_info_t tx_sort_map[MAX_PENDING_TXS];
    size_t tx_sort_count;
    
    
    executed_tx_t executed_txs[MAX_PENDING_TXS];
    size_t executed_count;
    
    
    mpt_tree_t token_registry;   
    
    
    mpt_tree_t token_trees[MAX_NODES];
    uint8_t token_addresses[MAX_NODES][42];
    size_t token_count;
    
    
    merkle_crdt_dag_t global_dag;  
    
    
    
    
    uint64_t last_sync_time;
    bool sync_in_progress;
    
    
    raft_state_t raft;
    
    
    uint64_t current_epoch;
    uint64_t epoch_start_time;
    bool epoch_in_progress;
    
    
    epoch_output_t epoch_outputs[MAX_CLUSTER_NODES];  
    size_t epoch_output_count;
    bool epoch_output_collected;  
    
    
    tee_network_state_t network;
    
    
    void* l2_full_node_state;  
} tee_cluster_state_t;

int tee_cluster_init(tee_cluster_state_t* cluster, uint32_t node_id);

int tee_cluster_register_token(tee_cluster_state_t* cluster,
                                const uint8_t* token_address,
                                const uint8_t* chain_id,
                                const uint8_t* deploy_tx_hash);

int tee_cluster_add_tx_request(tee_cluster_state_t* cluster,
                                const tx_request_t* tx);

int tee_cluster_elect_leader(tee_cluster_state_t* cluster);

int tee_cluster_set_tx_sort_info(tee_cluster_state_t* cluster,
                                  const tx_sort_info_t* sort_info,
                                  size_t count);

int tee_cluster_get_tx_sort_order(tee_cluster_state_t* cluster,
                                    uint64_t tx_id,
                                    uint64_t* sort_order);

int tee_cluster_start_epoch(tee_cluster_state_t* cluster, uint64_t epoch_id);

int tee_cluster_end_epoch(tee_cluster_state_t* cluster);

bool tee_cluster_is_epoch_complete(tee_cluster_state_t* cluster);

int tee_cluster_process_operation(tee_cluster_state_t* cluster,
                                   uint32_t chain_id,
                                   const operation_t* op);

int tee_cluster_process_operation_serial(tee_cluster_state_t* cluster,
                                         const operation_t* op,
                                         operation_t* tx_ops_cache,
                                         size_t* tx_ops_cache_count,
                                         size_t max_cache_size);

int tee_cluster_process_operations_serial_with_validation(tee_cluster_state_t* cluster,
                                                          const operation_t* operations,
                                                          size_t op_count);

int tee_cluster_init_l2_full_node(tee_cluster_state_t* cluster);

int tee_cluster_sync_l2_block_headers(tee_cluster_state_t* cluster,
                                      uint32_t chain_id,
                                      uint64_t from_block,
                                      uint64_t to_block);

int tee_cluster_distributed_verify_logs_merkle_crdt(
    tee_cluster_state_t* cluster,
    const void* logs,  
    size_t log_count,
    const void* proofs,  
    uint32_t tee_node_count,
    bool* verification_results);

bool tee_cluster_verify_log_existence(tee_cluster_state_t* cluster,
                                      const void* log,  
                                      const void* proof);  

int tee_cluster_process_log_with_verification(tee_cluster_state_t* cluster,
                                              uint32_t chain_id,
                                              const void* log,  
                                              const void* proof,  
                                              operation_t* op);

int tee_cluster_broadcast_dag_node(tee_cluster_state_t* cluster,
                                    uint32_t chain_id,
                                    uint64_t node_id);

int tee_cluster_request_dag_node(tee_cluster_state_t* cluster,
                                  uint32_t chain_id,
                                  uint64_t node_id);

int tee_cluster_generate_epoch_output(tee_cluster_state_t* cluster,
                                       uint8_t* mpt_root,
                                       uint8_t* dag_head,
                                       uint8_t* reject_root);

int tee_cluster_sync_dag(tee_cluster_state_t* cluster, uint32_t chain_id);

int tee_cluster_periodic_broadcast(tee_cluster_state_t* cluster);

int tee_cluster_leader_broadcast_tx_set(tee_cluster_state_t* cluster);

int tee_cluster_receive_and_sign_tx_set(tee_cluster_state_t* cluster,
                                          const executed_tx_t* tx_set,
                                          size_t count,
                                          uint8_t* signature);

int tee_cluster_add_executed_tx(tee_cluster_state_t* cluster,
                                 uint64_t tx_id,
                                 uint32_t chain_id,
                                 uint64_t block_number,
                                 uint64_t log_index);

int tee_cluster_listen_and_build_dag(tee_cluster_state_t* cluster,
                                      uint32_t chain_id,
                                      const operation_t* op);

int tee_cluster_generate_and_send_epoch_output(tee_cluster_state_t* cluster);

int tee_cluster_leader_collect_epoch_out

int tee_cluster_leader_sync_to_l2_chains(tee_cluster_state_t* cluster);

int tee_cluster_sync_node_from_other_tee(tee_cluster_state_t* local_cluster,
                                         const dag_node_t* remote_node);

int tee_cluster_sync_all_tee_dags(tee_cluster_state_t* local_cluster,
                                   const tee_cluster_state_t* remote_cluster);

#endif
