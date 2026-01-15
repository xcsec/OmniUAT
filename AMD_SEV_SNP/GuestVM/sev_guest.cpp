

#include "sev_guest.h"
#include "platform_sev.h"
#include "sev_vm_communication.h"
#include "../../Common/sequencer/sequencer.h"
#include "../../Common/mpt_tree/mpt_tree.h"
#include "../../Common/tee_cluster/tee_cluster.h"
#include "../../Common/merkle_crdt/merkle_crdt.h"
#include <string.h>
#include <stdio.h>

static sequencer_state_t g_sequencer_state;
static bool g_sequencer_initialized = false;

static tee_cluster_state_t g_cluster_state;
static bool g_cluster_initialized = false;

int sev_cluster_init(uint32_t node_id) {
    if (g_cluster_initialized) {

        return 0;
    }
    
    int ret = tee_cluster_init(&g_cluster_state, node_id);
    if (ret == 0) {
        g_cluster_initialized = true;

    } else {

    }
    
    return ret;
}

int sev_guest_init(void) {

    return 0;
}

int sev_guest_init_communication(uint32_t vm_id) {
    
    return sev_vm_comm_init(vm_id, SEV_COMM_CHANNEL_HOST_MEDIATED);
}

int sev_sequencer_init(void) {
    if (g_sequencer_initialized) {

        return 0;
    }
    
    int ret = sequencer_init(&g_sequencer_state);
    if (ret == 0) {
        g_sequencer_initialized = true;

    } else {

    }
    
    return ret;
}

int sev_sequencer_add_log(uint64_t timestamp,
                           uint32_t log_type,
                           const uint8_t* token_address,
                           const uint8_t* from,
                           const uint8_t* to,
                           const uint8_t* amount,
                           const uint8_t* signature) {
    if (!g_sequencer_initialized) {

        return -1;
    }
    
    if (token_address == NULL || from == NULL || to == NULL || 
        amount == NULL || signature == NULL) {
        return -1;
    }
    
    log_entry_t log;
    memset(&log, 0, sizeof(log_entry_t));
    
    log.timestamp = timestamp;
    log.type = (log_type_t)log_type;
    memcpy(log.token_address, token_address, 42);
    memcpy(log.from, from, 20);
    memcpy(log.to, to, 20);
    memcpy(log.amount, amount, 32);
    memcpy(log.signature, signature, 65);
    log.processed = false;
    
    int ret = sequencer_add_log(&g_sequencer_state, &log);
    if (ret == 0) {

    } else {

    }
    
    return ret;
}

int sev_sequencer_process_logs(void) {
    if (!g_sequencer_initialized) {

        return -1;
    }
    
    int ret = sequencer_process_logs(&g_sequencer_state);
    if (ret == 0) {

    } else {

    }
    
    return ret;
}

int sev_get_token_root(const uint8_t* token_address, uint8_t* root_hash) {
    if (!g_sequencer_initialized) {
        return -1;
    }
    
    if (token_address == NULL || root_hash == NULL) {
        return -1;
    }
    
    return sequencer_get_token_root(&g_sequencer_state, token_address, root_hash);
}

int sev_get_balance(const uint8_t* token_address,
                     const uint8_t* account,
                     uint8_t* balance) {
    if (!g_sequencer_initialized) {
        return -1;
    }
    
    if (token_address == NULL || account == NULL || balance == NULL) {
        return -1;
    }
    
    return sequencer_get_balance(&g_sequencer_state, token_address, account, balance);
}

int sev_register_token(const uint8_t* token_address,
                       const uint8_t* chain_id,
                       const uint8_t* deploy_tx_hash) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (token_address == NULL || chain_id == NULL || deploy_tx_hash == NULL) {
        return -1;
    }
    
    return tee_cluster_register_token(&g_cluster_state, token_address, 
                                       chain_id, deploy_tx_hash);
}

int sev_add_tx_request(uint64_t tx_id,
                        uint64_t timestamp,
                        const uint8_t* from,
                        const uint8_t* to,
                        const uint8_t* token_address,
                        const uint8_t* amount,
                        const uint8_t* signature,
                        uint32_t chain_id) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (from == NULL || to == NULL || token_address == NULL || 
        amount == NULL || signature == NULL) {
        return -1;
    }
    
    tx_request_t tx;
    memset(&tx, 0, sizeof(tx_request_t));
    tx.tx_id = tx_id;
    tx.timestamp = timestamp;
    memcpy(tx.from, from, 20);
    memcpy(tx.to, to, 20);
    memcpy(tx.token_address, token_address, 42);
    memcpy(tx.amount, amount, 32);
    memcpy(tx.signature, signature, 65);
    tx.chain_id = chain_id;
    tx.is_processed = false;
    
    return tee_cluster_add_tx_request(&g_cluster_state, &tx);
}

int sev_elect_leader(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_elect_leader(&g_cluster_state);
}

int sev_sort_txs(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    
    return 0;
}

int sev_process_operation(uint32_t chain_id,
                           uint64_t operation_id,
                           uint64_t tx_id,
                           uint64_t timestamp,
                           uint32_t op_type,
                           const uint8_t* token_address,
                           const uint8_t* account,
                           const uint8_t* amount) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (token_address == NULL || account == NULL || amount == NULL) {
        return -1;
    }
    
    operation_t op;
    memset(&op, 0, sizeof(operation_t));
    op.operation_id = operation_id;
    op.tx_id = tx_id;
    op.timestamp = timestamp;
    op.type = (operation_type_t)op_type;
    memcpy(op.token_address, token_address, 42);
    memcpy(op.account, account, 20);
    memcpy(op.amount, amount, 32);
    op.is_valid = true;
    
    return tee_cluster_process_operation(&g_cluster_state, chain_id, &op);
}

int sev_sync_dag(uint32_t chain_id) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_sync_dag(&g_cluster_state, chain_id);
}

int sev_generate_epoch_output(uint8_t* mpt_root,
                               uint8_t* dag_head,
                               uint8_t* reject_root) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (mpt_root == NULL || dag_head == NULL || reject_root == NULL) {
        return -1;
    }
    
    return tee_cluster_generate_epoch_output(&g_cluster_state, 
                                               mpt_root, dag_head, reject_root);
}

int sev_set_tx_sort_info(const uint64_t* tx_ids,
                          const uint64_t* sort_orders,
                          size_t count) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (tx_ids == NULL || sort_orders == NULL) {
        return -1;
    }
    
    
    tx_sort_info_t sort_info[MAX_PENDING_TXS];
    size_t info_count = (count > MAX_PENDING_TXS) ? MAX_PENDING_TXS : count;
    
    for (size_t i = 0; i < info_count; i++) {
        sort_info[i].tx_id = tx_ids[i];
        sort_info[i].sort_order = sort_orders[i];
        sort_info[i].sort_timestamp = 0; 
    }
    
    return tee_cluster_set_tx_sort_info(&g_cluster_state, sort_info, info_count);
}

int sev_add_executed_tx(uint64_t tx_id,
                         uint32_t chain_id,
                         uint64_t block_number,
                         uint64_t log_index) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_add_executed_tx(&g_cluster_state, tx_id, chain_id,
                                        block_number, log_index);
}

int sev_leader_broadcast_tx_set(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_leader_broadcast_tx_set(&g_cluster_state);
}

int sev_listen_and_build_dag(uint32_t chain_id,
                               uint64_t operation_id,
                               uint64_t tx_id,
                               uint64_t timestamp,
                               uint32_t op_type,
                               const uint8_t* token_address,
                               const uint8_t* account,
                               const uint8_t* amount) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    if (token_address == NULL || account == NULL || amount == NULL) {
        return -1;
    }
    
    operation_t op;
    memset(&op, 0, sizeof(operation_t));
    op.operation_id = operation_id;
    op.tx_id = tx_id;
    op.timestamp = timestamp;
    op.type = (operation_type_t)op_type;
    memcpy(op.token_address, token_address, 42);
    memcpy(op.account, account, 20);
    memcpy(op.amount, amount, 32);
    op.is_valid = true;
    
    return tee_cluster_listen_and_build_dag(&g_cluster_state, chain_id, &op);
}

int sev_generate_and_send_epoch_output(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_generate_and_send_epoch_output(&g_cluster_state);
}

int sev_leader_collect_epoch_outputs(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_leader_collect_epoch_out
}

int sev_leader_sync_to_l2_chains(void) {
    if (!g_cluster_initialized) {
        return -1;
    }
    
    return tee_cluster_leader_sync_to_l2_chains(&g_cluster_state);
}
