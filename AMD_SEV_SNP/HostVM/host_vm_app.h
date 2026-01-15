#ifndef _HOST_VM_APP_H_
#define _HOST_VM_APP_H_

#include <stdint.h>
#include <stddef.h>

int sev_guest_init(void);

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
