#ifndef _SEV_GUEST_H_
#define _SEV_GUEST_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

int sev_guest_init(void);

int sev_guest_init_communication(uint32_t vm_id);

int sev_cluster_init(uint32_t node_id);

int sev_register_token(const uint8_t* token_address,
                       const uint8_t* chain_id,
                       const uint8_t* deploy_tx_hash);

int sev_add_tx_request(uint64_t tx_id,
                        uint64_t timestamp,
                        const uint8_t* from,
                        const uint8_t* to,
                        const uint8_t* token_address,
                        const uint8_t* amount,
                        const uint8_t* signature,
                        uint32_t chain_id);

int sev_elect_leader(void);

int sev_sort_txs(void);

int sev_process_operation(uint32_t chain_id,
                           uint64_t operation_id,
                           uint64_t tx_id,
                           uint64_t timestamp,
                           uint32_t op_type,
                           const uint8_t* token_address,
                           const uint8_t* account,
                           const uint8_t* amount);

int sev_sync_dag(uint32_t chain_id);

int sev_generate_epoch_output(uint8_t* mpt_root,
                               uint8_t* dag_head,
                               uint8_t* reject_root);

int sev_set_tx_sort_info(const uint64_t* tx_ids,
                          const uint64_t* sort_orders,
                          size_t count);

int sev_add_executed_tx(uint64_t tx_id,
                         uint32_t chain_id,
                         uint64_t block_number,
                         uint64_t log_index);

int sev_leader_broadcast_tx_set(void);

int sev_listen_and_build_dag(uint32_t chain_id,
                               uint64_t operation_id,
                               uint64_t tx_id,
                               uint64_t timestamp,
                               uint32_t op_type,
                               const uint8_t* token_address,
                               const uint8_t* account,
                               const uint8_t* amount);

int sev_generate_and_send_epoch_output(void);

int sev_leader_collect_epoch_out

int sev_leader_sync_to_l2_chains(void);

int sev_get_token_root(const uint8_t* token_address, uint8_t* root_hash);

int sev_get_balance(const uint8_t* token_address,
                     const uint8_t* account,
                     uint8_t* balance);

int sev_sequencer_init(void);

int sev_sequencer_add_log(uint64_t timestamp,
                           uint32_t log_type,
                           const uint8_t* token_address,
                           const uint8_t* from,
                           const uint8_t* to,
                           const uint8_t* amount,
                           const uint8_t* signature);

int sev_sequencer_process_logs(void);

#endif
