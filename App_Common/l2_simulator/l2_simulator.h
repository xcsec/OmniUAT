#ifndef _L2_SIMULATOR_H_
#define _L2_SIMULATOR_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_SIMULATED_CHAINS 16
#define MAX_PENDING_OPERATIONS 1000

typedef struct {
    uint32_t chain_id;
    char chain_name[64];
    bool is_running;
    uint64_t block_number;
    uint64_t tx_counter;
} l2_chain_simulator_t;

typedef void (*operation_callback_t)(uint32_t chain_id,
                                      uint64_t operation_id,
                                      uint64_t tx_id,
                                      uint64_t timestamp,
                                      uint32_t op_type,
                                      const uint8_t* token_address,
                                      const uint8_t* account,
                                      const uint8_t* amount);

typedef struct {
    l2_chain_simulator_t chains[MAX_SIMULATED_CHAINS];
    size_t chain_count;
    operation_callback_t callback;
    bool running;
} l2_simulator_manager_t;

int l2_simulator_init(l2_simulator_manager_t* simulator);

int l2_simulator_add_chain(l2_simulator_manager_t* simulator,
                            uint32_t chain_id,
                            const char* chain_name);

void l2_simulator_set_callback(l2_simulator_manager_t* simulator,
                                operation_callback_t callback);

int l2_simulator_generate_transfer(l2_simulator_manager_t* simulator,
                                    uint32_t chain_id,
                                    const uint8_t* token_address,
                                    const uint8_t* from,
                                    const uint8_t* to,
                                    const uint8_t* amount);

int l2_simulator_generate_mint(l2_simulator_manager_t* simulator,
                                 uint32_t chain_id,
                                 const uint8_t* token_address,
                                 const uint8_t* to,
                                 const uint8_t* amount);

int l2_simulator_generate_burn(l2_simulator_manager_t* simulator,
                                 uint32_t chain_id,
                                 const uint8_t* token_address,
                                 const uint8_t* from,
                                 const uint8_t* amount);

int l2_simulator_start(l2_simulator_manager_t* simulator);

void l2_simulator_stop(l2_simulator_manager_t* simulator);

void l2_simulator_generate_random_address(uint8_t* address);

void l2_simulator_generate_random_token(uint8_t* token_address);

#endif
