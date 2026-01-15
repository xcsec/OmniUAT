#ifndef _RAFT_NETWORK_H_
#define _RAFT_NETWORK_H_

#include "raft.h"
#include "../tee_network/tee_network.h"
#include <stdint.h>
#include <stddef.h>

int raft_serialize_message(const raft_message_t* msg, uint8_t* buffer, size_t* size, size_t max_size);

int raft_deserialize_message(const uint8_t* buffer, size_t size, raft_message_t* msg);

int raft_send_message(tee_network_state_t* network, const raft_message_t* msg);

int raft_receive_message(tee_network_state_t* network, raft_message_t* msg);

int raft_send_request_vote(tee_network_state_t* network, 
                           uint32_t to_node_id,
                           uint64_t term,
                           uint64_t last_log_index,
                           uint64_t last_log_term);

int raft_send_append_entries(tee_network_state_t* network,
                              uint32_t to_node_id,
                              uint64_t term,
                              uint64_t prev_log_index,
                              uint64_t prev_log_term,
                              uint64_t leader_commit,
                              const raft_log_entry_t* entries,
                              size_t entry_count);

int raft_tick_with_network(raft_state_t* raft, tee_network_state_t* network);

#endif
