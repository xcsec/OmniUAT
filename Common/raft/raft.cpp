#include "raft.h"
#include "../mpt_tree/mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

static int get_hw_random(uint64_t* random) {
    if (random == NULL) return -1;
    return platform_get_random((uint8_t*)random, sizeof(uint64_t));
}

static uint64_t get_time_ms(void) {
    
    static uint64_t time_counter = 0;
    return ++time_counter;
}

static uint64_t random_election_timeout(void) {
    uint64_t random;
    if (get_hw_random(&random) != 0) {
        random = 200; 
    }
    return RAFT_ELECTION_TIMEOUT_MIN + (random % (RAFT_ELECTION_TIMEOUT_MAX - RAFT_ELECTION_TIMEOUT_MIN));
}

int raft_init(raft_state_t* raft, uint32_t node_id) {
    if (raft == NULL) return -1;
    
    memset(raft, 0, sizeof(raft_state_t));
    raft->my_node_id = node_id;
    raft->role = RAFT_FOLLOWER;
    raft->current_term = 0;
    raft->voted_for = 0;
    raft->log_size = 0;
    raft->commit_index = 0;
    raft->last_applied = 0;
    raft->leader_id = 0;
    raft->current_epoch = 0;
    raft->epoch_in_progress = false;
    raft->election_timeout = random_election_timeout();
    raft->last_heartbeat = get_time_ms();
    
    return 0;
}

static void become_follower(raft_state_t* raft, uint64_t term) {
    raft->role = RAFT_FOLLOWER;
    raft->current_term = term;
    raft->voted_for = 0;
    raft->leader_id = 0;
    raft->election_timeout = random_election_timeout();
    raft->last_heartbeat = get_time_ms();
}

static void become_candidate(raft_state_t* raft) {
    raft->role = RAFT_CANDIDATE;
    raft->current_term++;
    raft->voted_for = raft->my_node_id;
    raft->leader_id = 0;
    raft->election_timeout = random_election_timeout();
    raft->last_heartbeat = get_time_ms();
}

static void become_leader(raft_state_t* raft) {
    raft->role = RAFT_LEADER;
    raft->leader_id = raft->my_node_id;
    raft->last_heartbeat = get_time_ms();
    
    
    for (size_t i = 0; i < raft->peer_count; i++) {
        raft->peers[i].next_index = raft->log_size + 1;
        raft->peers[i].match_index = 0;
    }
}

static int handle_request_vote(raft_state_t* raft, const raft_message_t* msg) {
    raft_message_t response;
    memset(&response, 0, sizeof(raft_message_t));
    response.type = RAFT_MSG_REQUEST_VOTE_RESPONSE;
    response.from_node_id = raft->my_node_id;
    response.to_node_id = msg->from_node_id;
    response.term = raft->current_term;
    response.vote_granted = false;
    
    
    if (msg->term > raft->current_term) {
        become_follower(raft, msg->term);
        response.term = raft->current_term;
    }
    
    
    bool can_vote = (raft->voted_for == 0 || raft->voted_for == msg->from_node_id) &&
                    (msg->term >= raft->current_term);
    
    
    bool log_ok = (msg->candidate_last_log_term > raft->log[raft->log_size - 1].term) ||
                  (msg->candidate_last_log_term == raft->log[raft->log_size - 1].term &&
                   msg->candidate_last_log_index >= raft->log_size);
    
    if (can_vote && log_ok && msg->term == raft->current_term) {
        raft->voted_for = msg->from_node_id;
        response.vote_granted = true;
    }
    
    
    return 0;
}

static int handle_append_entries(raft_state_t* raft, const raft_message_t* msg) {
    raft_message_t response;
    memset(&response, 0, sizeof(raft_message_t));
    response.type = RAFT_MSG_APPEND_ENTRIES_RESPONSE;
    response.from_node_id = raft->my_node_id;
    response.to_node_id = msg->from_node_id;
    response.term = raft->current_term;
    response.success = false;
    
    
    if (msg->term > raft->current_term) {
        become_follower(raft, msg->term);
        response.term = raft->current_term;
    }
    
    
    if (msg->term == raft->current_term) {
        if (raft->role == RAFT_CANDIDATE) {
            become_follower(raft, msg->term);
        }
        raft->leader_id = msg->from_node_id;
        raft->last_heartbeat = get_time_ms();
    }
    
    
    bool log_match = false;
    if (msg->prev_log_index == 0) {
        log_match = true;
    } else if (msg->prev_log_index <= raft->log_size) {
        if (raft->log[msg->prev_log_index - 1].term == msg->prev_log_term) {
            log_match = true;
        }
    }
    
    if (msg->term == raft->current_term && log_match) {
        
        for (size_t i = 0; i < msg->entry_count; i++) {
            if (msg->prev_log_index + i < raft->log_size) {
                
                if (raft->log[msg->prev_log_index + i].term != msg->entries[i].term) {
                    
                    raft->log_size = msg->prev_log_index + i;
                }
            }
            
            if (msg->prev_log_index + i >= raft->log_size) {
                
                if (raft->log_size >= MAX_LOG_ENTRIES) {
                    return -1;
                }
                memcpy(&raft->log[raft->log_size], &msg->entries[i], sizeof(raft_log_entry_t));
                raft->log_size++;
            }
        }
        
        
        if (msg->leader_commit > raft->commit_index) {
            uint64_t new_commit = msg->leader_commit;
            if (new_commit > raft->log_size) {
                new_commit = raft->log_size;
            }
            raft->commit_index = new_commit;
        }
        
        response.success = true;
        response.match_index = raft->log_size;
    }
    
    
    return 0;
}

int raft_process_message(raft_state_t* raft, const raft_message_t* msg) {
    if (raft == NULL || msg == NULL) return -1;
    
    
    if (msg->term > raft->current_term) {
        become_follower(raft, msg->term);
    }
    
    switch (msg->type) {
        case RAFT_MSG_REQUEST_VOTE:
            return handle_request_vote(raft, msg);
            
        case RAFT_MSG_APPEND_ENTRIES:
        case RAFT_MSG_HEARTBEAT:
            return handle_append_entries(raft, msg);
            
        default:
            return -1;
    }
}

int raft_append_entry(raft_state_t* raft, const raft_log_entry_t* entry) {
    if (raft == NULL || entry == NULL) return -1;
    
    if (raft->role != RAFT_LEADER) {
        return -1; 
    }
    
    if (raft->log_size >= MAX_LOG_ENTRIES) {
        return -1;
    }
    
    
    raft_log_entry_t log_entry;
    memcpy(&log_entry, entry, sizeof(raft_log_entry_t));
    log_entry.term = raft->current_term;
    log_entry.index = raft->log_size + 1;
    
    
    memcpy(&raft->log[raft->log_size], &log_entry, sizeof(raft_log_entry_t));
    raft->log_size++;
    
    
    
    
    return 0;
}

uint32_t raft_get_leader(raft_state_t* raft) {
    if (raft == NULL) return 0;
    return raft->leader_id;
}

bool raft_is_leader(raft_state_t* raft) {
    if (raft == NULL) return false;
    return raft->role == RAFT_LEADER;
}

int raft_get_committed_entries(raft_state_t* raft, 
                                raft_log_entry_t* entries, 
                                size_t* count, 
                                size_t max_count) {
    if (raft == NULL || entries == NULL || count == NULL) return -1;
    
    *count = 0;
    
    
    for (uint64_t i = raft->last_applied; i < raft->commit_index && *count < max_count; i++) {
        if (i < raft->log_size) {
            memcpy(&entries[*count], &raft->log[i], sizeof(raft_log_entry_t));
            (*count)++;
        }
    }
    
    return 0;
}

int raft_start_epoch(raft_state_t* raft, uint64_t epoch_id) {
    if (raft == NULL) return -1;
    
    if (raft->epoch_in_progress) {
        return -1; 
    }
    
    raft->current_epoch = epoch_id;
    raft->epoch_start_time = get_time_ms();
    raft->epoch_in_progress = true;
    
    return 0;
}

int raft_end_epoch(raft_state_t* raft) {
    if (raft == NULL) return -1;
    
    if (!raft->epoch_in_progress) {
        return -1; 
    }
    
    raft->epoch_in_progress = false;
    raft->last_applied = raft->commit_index; 
    
    return 0;
}

bool raft_is_epoch_complete(raft_state_t* raft) {
    if (raft == NULL) return false;
    
    
    return raft->epoch_in_progress && (raft->last_applied >= raft->commit_index);
}

int raft_tick(raft_state_t* raft) {
    if (raft == NULL) return -1;
    
    uint64_t current_time = get_time_ms();
    
    switch (raft->role) {
        case RAFT_FOLLOWER:
        case RAFT_CANDIDATE: {
            
            if (current_time - raft->last_heartbeat > raft->election_timeout) {
                
                become_candidate(raft);
                
            }
            break;
        }
        
        case RAFT_LEADER: {
            
            if (current_time - raft->last_heartbeat > RAFT_HEARTBEAT_INTERVAL) {
                raft->last_heartbeat = current_time;
                
            }
            break;
        }
    }
    
    return 0;
}

int raft_add_peer(raft_state_t* raft, uint32_t node_id) {
    if (raft == NULL) return -1;
    
    if (raft->peer_count >= MAX_RAFT_NODES) {
        return -1;
    }
    
    
    for (size_t i = 0; i < raft->peer_count; i++) {
        if (raft->peers[i].node_id == node_id) {
            return 0; 
        }
    }
    
    
    raft->peers[raft->peer_count].node_id = node_id;
    raft->peers[raft->peer_count].next_index = raft->log_size + 1;
    raft->peers[raft->peer_count].match_index = 0;
    raft->peers[raft->peer_count].is_active = true;
    raft->peers[raft->peer_count].last_heartbeat = 0;
    raft->peer_count++;
    
    return 0;
}
