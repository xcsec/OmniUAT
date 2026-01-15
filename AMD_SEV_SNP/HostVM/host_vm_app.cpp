

#include "host_vm_app.h"
#include "../../App_Common/l2_simulator/l2_simulator.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static bool g_guest_initialized = false;

int sev_guest_init(void) {
    if (g_guest_initialized) {
        return 0;
    }
    
    
    
    
    
    g_guest_initialized = true;

    return 0;
}

static int invoke_guest(uint32_t function_id, void* params, size_t param_size,
                         void* result, size_t* result_size) {
    if (!g_guest_initialized) return -1;
    
    
    
    
    
    
    
    return 0;
}

int sev_register_token(const uint8_t* token_address,
                       const uint8_t* chain_id,
                       const uint8_t* deploy_tx_hash) {
    uint8_t params[78];
    memcpy(params, token_address, 42);
    memcpy(params + 42, chain_id, 4);
    memcpy(params + 46, deploy_tx_hash, 32);
    return invoke_guest(1, params, 78, NULL, NULL);
}

int sev_add_tx_request(uint64_t tx_id,
                        uint64_t timestamp,
                        const uint8_t* from,
                        const uint8_t* to,
                        const uint8_t* token_address,
                        const uint8_t* amount,
                        const uint8_t* signature,
                        uint32_t chain_id) {
    
    return invoke_guest(2, NULL, 0, NULL, NULL);
}

int sev_elect_leader(void) {
    return invoke_guest(3, NULL, 0, NULL, NULL);
}

int sev_sort_txs(void) {
    return invoke_guest(4, NULL, 0, NULL, NULL);
}

int sev_process_operation(uint32_t chain_id,
                           uint64_t operation_id,
                           uint64_t tx_id,
                           uint64_t timestamp,
                           uint32_t op_type,
                           const uint8_t* token_address,
                           const uint8_t* account,
                           const uint8_t* amount) {
    
    return invoke_guest(5, NULL, 0, NULL, NULL);
}

int sev_sync_dag(uint32_t chain_id) {
    return invoke_guest(6, NULL, 0, NULL, NULL);
}

int sev_generate_epoch_output(uint8_t* mpt_root,
                               uint8_t* dag_head,
                               uint8_t* reject_root) {
    size_t result_size = 96;  
    uint8_t result[96];
    int ret = invoke_guest(7, NULL, 0, result, &result_size);
    if (ret == 0) {
        memcpy(mpt_root, result, 32);
        memcpy(dag_head, result + 32, 32);
        memcpy(reject_root, result + 64, 32);
    }
    return ret;
}

int sev_get_token_root(const uint8_t* token_address, uint8_t* root_hash) {
    size_t result_size = 32;
    return invoke_guest(8, (void*)token_address, 42, root_hash, &result_size);
}

int sev_get_balance(const uint8_t* token_address,
                     const uint8_t* account,
                     uint8_t* balance) {
    size_t result_size = 32;
    uint8_t params[62];
    memcpy(params, token_address, 42);
    memcpy(params + 42, account, 20);
    return invoke_guest(9, params, 62, balance, &result_size);
}

int sev_sequencer_init(void) {
    return invoke_guest(10, NULL, 0, NULL, NULL);
}

int sev_sequencer_add_log(uint64_t timestamp,
                           uint32_t log_type,
                           const uint8_t* token_address,
                           const uint8_t* from,
                           const uint8_t* to,
                           const uint8_t* amount,
                           const uint8_t* signature) {
    
    return invoke_guest(11, NULL, 0, NULL, NULL);
}

int sev_sequencer_process_logs(void) {
    return invoke_guest(12, NULL, 0, NULL, NULL);
}

int sev_cluster_init(uint32_t node_id) {
    uint32_t params = node_id;
    return invoke_guest(13, &params, sizeof(uint32_t), NULL, NULL);
}

static void operation_callback(uint32_t chain_id,
                                uint64_t operation_id,
                                uint64_t tx_id,
                                uint64_t timestamp,
                                uint32_t op_type,
                                const uint8_t* token_address,
                                const uint8_t* account,
                                const uint8_t* amount) {

    sev_process_operation(chain_id, operation_id, tx_id, timestamp,
                           op_type, token_address, account, amount);
}

static int demo_l2_simulator_sev(void) {

    if (sev_guest_init() < 0) {

        return -1;
    }

    if (sev_cluster_init(1) < 0) {

        return -1;
    }

    uint8_t token1[42] = {'0','x','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1'};
    uint8_t token2[42] = {'0','x','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2','2'};
    uint8_t chain_id1[4] = {0, 0, 0, 1};
    uint8_t chain_id2[4] = {0, 0, 0, 2};
    uint8_t deploy_hash[32] = {0x01, 0x02, 0x03};
    
    sev_register_token(token1, chain_id1, deploy_hash);
    sev_register_token(token2, chain_id2, deploy_hash);
    
    
    l2_simulator_manager_t simulator;
    l2_simulator_init(&simulator);
    l2_simulator_add_chain(&simulator, 1, "Arbitrum");
    l2_simulator_add_chain(&simulator, 2, "Optimism");
    l2_simulator_set_callback(&simulator, operation_callback);

    if (l2_simulator_start(&simulator) != 0) {

        return -1;
    }
    
    
    for (int i = 0; i < 10; i++) {
        sleep(2);

        sev_sync_dag(1);
        sev_sync_dag(2);
        
        
        uint8_t mpt_root[32], dag_head[32], reject_root[32];
        if (sev_generate_epoch_output(mpt_root, dag_head, reject_root) == 0) {

            for (int j = 0; j < 32; j++)

        }
    }

    l2_simulator_stop(&simulator);
    
    return 0;
}

int main(int argc, char* argv[]) {

    bool run_demo = false;
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        run_demo = true;
    }
    
    
    if (sev_guest_init() < 0) {

        return -1;
    }
    
    if (run_demo) {
        
        return demo_l2_simulator_sev();
    }

    if (sev_sequencer_init() < 0) {

        return -1;
    }

    uint8_t token_addr[42] = {'0','x','1','2','3','4','5','6','7','8','9','0','1','2','3','4','5','6','7','8','9','0','1','2','3','4','5','6','7','8','9','0','1','2','3','4','5','6','7','8','9','0'};
    uint8_t from_addr[20] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                              0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
                              0x11, 0x22, 0x33, 0x44};
    uint8_t to_addr[20] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
                           0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                           0xaa, 0xbb, 0xcc, 0xdd};
    uint8_t amount[32] = {0};
    amount[31] = 100;
    uint8_t signature[65] = {0x01, 0x02, 0x03};
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    
    if (sev_sequencer_add_log(timestamp, 0, token_addr, from_addr, to_addr, 
                              amount, signature) == 0) {

    }

    if (sev_sequencer_process_logs() == 0) {

    }

    uint8_t root_hash[32];
    if (sev_get_token_root(token_addr, root_hash) == 0) {

        for (int i = 0; i < 32; i++) {

        }

    }

    return 0;
}
