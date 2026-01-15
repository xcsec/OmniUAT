#include "raft_network.h"
#include "raft.h"
#include "../tee_network/tee_network.h"
#include <string.h>

int raft_serialize_message(const raft_message_t* msg, uint8_t* buffer, size_t* size, size_t max_size) {
    if (msg == NULL || buffer == NULL || size == NULL) return -1;
    
    size_t offset = 0;
    
    
    if (offset + sizeof(raft_message_type_t) > max_size) return -1;
    memcpy(buffer + offset, &msg->type, sizeof(raft_message_type_t));
    offset += sizeof(raft_message_type_t);
    
    
    if (offset + sizeof(uint32_t) * 2 + sizeof(uint64_t) * 5 > max_size) return -1;
    memcpy(buffer + offset, &msg->from_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &msg->to_node_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &msg->term, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &msg->candidate_last_log_index, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &msg->candidate_last_log_term, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &msg->prev_log_index, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &msg->prev_log_term, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buffer + offset, &msg->leader_commit, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    if (offset + sizeof(bool) * 2 + sizeof(uint64_t) > max_size) return -1;
    memcpy(buffer + offset, &msg->vote_granted, sizeof(bool));
    offset += sizeof(bool);
    memcpy(buffer + offset, &msg->success, sizeof(bool));
    offset += sizeof(bool);
    memcpy(buffer + offset, &msg->match_index, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    if (offset + sizeof(size_t) > max_size) return -1;
    memcpy(buffer + offset, &msg->entry_count, sizeof(size_t));
    offset += sizeof(size_t);
    
    for (size_t i = 0; i < msg->entry_count; i++) {
        if (offset + sizeof(raft_log_entry_t) > max_size) return -1;
        memcpy(buffer + offset, &msg->entries[i], sizeof(raft_log_entry_t));
        offset += sizeof(raft_log_entry_t);
    }
    
    *size = offset;
    return 0;
}

int raft_deserialize_message(const uint8_t* buffer, size_t size, raft_message_t* msg) {
    if (buffer == NULL || msg == NULL) return -1;
    
    size_t offset = 0;
    
    
    if (offset + sizeof(raft_message_type_t) > size) return -1;
    memcpy(&msg->type, buffer + offset, sizeof(raft_message_type_t));
    offset += sizeof(raft_message_type_t);
    
    
    if (offset + sizeof(uint32_t) * 2 + sizeof(uint64_t) * 5 > size) return -1;
    memcpy(&msg->from_node_id, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&msg->to_node_id, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&msg->term, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&msg->candidate_last_log_index, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&msg->candidate_last_log_term, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&msg->prev_log_index, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&msg->prev_log_term, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&msg->leader_commit, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    if (offset + sizeof(bool) * 2 + sizeof(uint64_t) > size) return -1;
    memcpy(&msg->vote_granted, buffer + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&msg->success, buffer + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&msg->match_index, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    
    if (offset + sizeof(size_t) > size) return -1;
    memcpy(&msg->entry_count, buffer + offset, sizeof(size_t));
    offset += sizeof(size_t);
    
    if (msg->entry_count > 100) return -1; 
    
    for (size_t i = 0; i < msg->entry_count; i++) {
        if (offset + sizeof(raft_log_entry_t) > size) return -1;
        memcpy(&msg->entries[i], buffer + offset, sizeof(raft_log_entry_t));
        offset += sizeof(raft_log_entry_t);
    }
    
    return 0;
}

int raft_send_message(tee_network_state_t* network, const raft_message_t* msg) {
    if (network == NULL || msg == NULL) return -1;
    
    
    uint8_t buffer[4096];
    size_t buffer_size = 0;
    
    if (raft_serialize_message(msg, buffer, &buffer_size, sizeof(buffer)) != 0) {
        return -1;
    }
    
    
    message_type_t network_type;
    switch (msg->type) {
        case RAFT_MSG_REQUEST_VOTE:
            network_type = MSG_RAFT_REQUEST_VOTE;
            break;
        case RAFT_MSG_REQUEST_VOTE_RESPONSE:
            network_type = MSG_RAFT_REQUEST_VOTE_RESPONSE;
            break;
        case RAFT_MSG_APPEND_ENTRIES:
            network_type = MSG_RAFT_APPEND_ENTRIES;
            break;
        case RAFT_MSG_APPEND_ENTRIES_RESPONSE:
            network_type = MSG_RAFT_APPEND_ENTRIES_RESPONSE;
            break;
        default:
            return -1;
    }
    
    
    return tee_network_send_message(network, msg->to_node_id, network_type, buffer, buffer_size);
}

int raft_receive_message(tee_network_state_t* network, raft_message_t* msg) {
    if (network == NULL || msg == NULL) return -1;
    
    
    tee_message_t tee_msg;
    if (tee_network_receive_message(network, &tee_msg) != 0) {
        return -1;  
    }
    
    
    message_type_t network_type = tee_msg.header.type;
    raft_message_type_t raft_type;
    
    switch (network_type) {
        case MSG_RAFT_REQUEST_VOTE:
            raft_type = RAFT_MSG_REQUEST_VOTE;
            break;
        case MSG_RAFT_REQUEST_VOTE_RESPONSE:
            raft_type = RAFT_MSG_REQUEST_VOTE_RESPONSE;
            break;
        case MSG_RAFT_APPEND_ENTRIES:
            raft_type = RAFT_MSG_APPEND_ENTRIES;
            break;
        case MSG_RAFT_APPEND_ENTRIES_RESPONSE:
            raft_type = RAFT_MSG_APPEND_ENTRIES_RESPONSE;
            break;
        default:
            return -1;  
    }
    
    
    raft_message_t raft_msg;
    if (raft_deserialize_message(tee_msg.payload, tee_msg.header.payload_size, &raft_msg) != 0) {
        return -1;
    }
    
    
    raft_msg.type = raft_type;
    raft_msg.from_node_id = tee_msg.header.from_node_id;
    
    memcpy(msg, &raft_msg, sizeof(raft_message_t));
    return 0;
}

int raft_send_request_vote(tee_network_state_t* network, 
                           uint32_t to_node_id,
                           uint64_t term,
                           uint64_t last_log_index,
                           uint64_t last_log_term) {
    if (network == NULL) return -1;
    
    raft_message_t msg;
    memset(&msg, 0, sizeof(raft_message_t));
    msg.type = RAFT_MSG_REQUEST_VOTE;
    msg.from_node_id = network->my_node_id;
    msg.to_node_id = to_node_id;
    msg.term = term;
    msg.candidate_last_log_index = last_log_index;
    msg.candidate_last_log_term = last_log_term;
    
    return raft_send_message(network, &msg);
}

int raft_send_append_entries(tee_network_state_t* network,
                              uint32_t to_node_id,
                              uint64_t term,
                              uint64_t prev_log_index,
                              uint64_t prev_log_term,
                              uint64_t leader_commit,
                              const raft_log_entry_t* entries,
                              size_t entry_count) {
    if (network == NULL) return -1;
    
    raft_message_t msg;
    memset(&msg, 0, sizeof(raft_message_t));
    msg.type = RAFT_MSG_APPEND_ENTRIES;
    msg.from_node_id = network->my_node_id;
    msg.to_node_id = to_node_id;
    msg.term = term;
    msg.prev_log_index = prev_log_index;
    msg.prev_log_term = prev_log_term;
    msg.leader_commit = leader_commit;
    
    if (entry_count > 100) entry_count = 100;
    msg.entry_count = entry_count;
    if (entries != NULL && entry_count > 0) {
        memcpy(msg.entries, entries, entry_count * sizeof(raft_log_entry_t));
    }
    
    return raft_send_message(network, &msg);
}

int raft_tick_with_network(raft_state_t* raft, tee_network_state_t* network) {
    if (raft == NULL || network == NULL) return -1;
    
    
    extern int raft_tick(raft_state_t* raft);
    if (raft_tick(raft) != 0) {
        return -1;
    }
    
    
    raft_message_t msg;
    extern int raft_process_message(raft_state_t* raft, const raft_message_t* msg);
    while (raft_receive_message(network, &msg) == 0) {
        raft_process_message(raft, &msg);
    }
    
    
    if (raft->role == RAFT_CANDIDATE) {
        uint64_t last_log_index = raft->log_size;
        uint64_t last_log_term = (last_log_index > 0) ? raft->log[last_log_index - 1].term : 0;
        
        for (size_t i = 0; i < raft->peer_count; i++) {
            if (raft->peers[i].is_active) {
                raft_send_request_vote(network, raft->peers[i].node_id,
                                       raft->current_term, last_log_index, last_log_term);
            }
        }
    }
    
    
    if (raft->role == RAFT_LEADER) {
        for (size_t i = 0; i < raft->peer_count; i++) {
            if (raft->peers[i].is_active) {
                uint64_t prev_log_index = raft->peers[i].next_index - 1;
                uint64_t prev_log_term = (prev_log_index > 0) ? raft->log[prev_log_index - 1].term : 0;
                
                
                raft_log_entry_t entries[100];
                size_t entry_count = 0;
                
                if (raft->peers[i].next_index < raft->log_size) {
                    for (uint64_t j = raft->peers[i].next_index; j < raft->log_size && entry_count < 100; j++) {
                        memcpy(&entries[entry_count], &raft->log[j], sizeof(raft_log_entry_t));
                        entry_count++;
                    }
                }
                
                raft_send_append_entries(network, raft->peers[i].node_id,
                                        raft->current_term, prev_log_index, prev_log_term,
                                        raft->commit_index, entries, entry_count);
            }
        }
    }
    
    return 0;
}
