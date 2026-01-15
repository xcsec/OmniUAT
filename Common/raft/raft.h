#ifndef _RAFT_H_
#define _RAFT_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_RAFT_NODES 16
#define MAX_LOG_ENTRIES 100000
#define RAFT_ELECTION_TIMEOUT_MIN 150  
#define RAFT_ELECTION_TIMEOUT_MAX 300  
#define RAFT_HEARTBEAT_INTERVAL 50    

typedef enum {
    RAFT_FOLLOWER = 0,
    RAFT_CANDIDATE = 1,
    RAFT_LEADER = 2
} raft_role_t;

typedef struct {
    uint64_t term;          
    uint64_t index;         
    uint64_t tx_id;         
    uint64_t timestamp;     
    uint8_t data[256];      
    size_t data_size;       
} raft_log_entry_t;

typedef struct {
    uint32_t node_id;
    uint64_t next_index;    
    uint64_t match_index;    
    bool is_active;
    uint64_t last_heartbeat; 
} raft_peer_t;

typedef struct {
    
    uint64_t current_term;      
    uint32_t voted_for;         
    
    
    raft_role_t role;           
    uint64_t last_heartbeat;    
    uint64_t election_timeout;  
    
    
    raft_log_entry_t log[MAX_LOG_ENTRIES];
    uint64_t log_size;          
    uint64_t commit_index;      
    uint64_t last_applied;      
    
    
    raft_peer_t peers[MAX_RAFT_NODES];
    size_t peer_count;
    
    
    uint32_t my_node_id;
    uint32_t leader_id;         
    
    
    uint64_t current_epoch;     
    uint64_t epoch_start_time;  
    bool epoch_in_progress;     
} raft_state_t;

typedef enum {
    RAFT_MSG_REQUEST_VOTE = 1,
    RAFT_MSG_REQUEST_VOTE_RESPONSE = 2,
    RAFT_MSG_APPEND_ENTRIES = 3,
    RAFT_MSG_APPEND_ENTRIES_RESPONSE = 4,
    RAFT_MSG_HEARTBEAT = 5
} raft_message_type_t;

typedef struct {
    raft_message_type_t type;
    uint32_t from_node_id;
    uint32_t to_node_id;
    uint64_t term;
    
    
    uint64_t candidate_last_log_index;
    uint64_t candidate_last_log_term;
    
    
    uint64_t prev_log_index;
    uint64_t prev_log_term;
    uint64_t leader_commit;
    raft_log_entry_t entries[100];  
    size_t entry_count;
    
    
    bool vote_granted;
    bool success;
    uint64_t match_index;
} raft_message_t;

int raft_init(raft_state_t* raft, uint32_t node_id);

int raft_process_message(raft_state_t* raft, const raft_message_t* msg);

int raft_append_entry(raft_state_t* raft, const raft_log_entry_t* entry);

uint32_t raft_get_leader(raft_state_t* raft);

bool raft_is_leader(raft_state_t* raft);

int raft_get_committed_entries(raft_state_t* raft, 
                                raft_log_entry_t* entries, 
                                size_t* count, 
                                size_t max_count);

int raft_start_epoch(raft_state_t* raft, uint64_t epoch_id);

int raft_end_epoch(raft_state_t* raft);

bool raft_is_epoch_complete(raft_state_t* raft);

int raft_tick(raft_state_t* raft);

int raft_add_peer(raft_state_t* raft, uint32_t node_id);

#endif
