#include "l2_simulator.h"
#include "App.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

static l2_simulator_manager_t* g_simulator = NULL;
static pthread_t g_simulator_thread;
static bool g_thread_running = false;

void l2_simulator_generate_random_address(uint8_t* address) {
    if (address == NULL) return;
    
    for (int i = 0; i < 20; i++) {
        address[i] = rand() % 256;
    }
}

void l2_simulator_generate_random_token(uint8_t* token_address) {
    if (token_address == NULL) return;
    
    
    token_address[0] = '0';
    token_address[1] = 'x';
    
    for (int i = 2; i < 42; i++) {
        int hex_digit = rand() % 16;
        if (hex_digit < 10) {
            token_address[i] = '0' + hex_digit;
        } else {
            token_address[i] = 'a' + (hex_digit - 10);
        }
    }
}

static void generate_random_amount(uint8_t* amount) {
    if (amount == NULL) return;
    
    memset(amount, 0, 32);
    
    uint64_t value = (rand() % 1000) + 1;
    
    amount[31] = value & 0xFF;
    amount[30] = (value >> 8) & 0xFF;
    amount[29] = (value >> 16) & 0xFF;
    amount[28] = (value >> 24) & 0xFF;
}

int l2_simulator_init(l2_simulator_manager_t* simulator) {
    if (simulator == NULL) return -1;
    
    memset(simulator, 0, sizeof(l2_simulator_manager_t));
    simulator->running = false;
    g_simulator = simulator;
    
    srand(time(NULL));
    
    return 0;
}

int l2_simulator_add_chain(l2_simulator_manager_t* simulator,
                            uint32_t chain_id,
                            const char* chain_name) {
    if (simulator == NULL || chain_name == NULL) return -1;
    
    if (simulator->chain_count >= MAX_SIMULATED_CHAINS) return -1;
    
    size_t idx = simulator->chain_count;
    simulator->chains[idx].chain_id = chain_id;
    strncpy(simulator->chains[idx].chain_name, chain_name, 63);
    simulator->chains[idx].is_running = false;
    simulator->chains[idx].block_number = 0;
    simulator->chains[idx].tx_counter = 0;
    
    simulator->chain_count++;
    
    
    return 0;
}

void l2_simulator_set_callback(l2_simulator_manager_t* simulator,
                                operation_callback_t callback) {
    if (simulator == NULL) return;
    
    simulator->callback = callback;
}

int l2_simulator_generate_transfer(l2_simulator_manager_t* simulator,
                                    uint32_t chain_id,
                                    const uint8_t* token_address,
                                    const uint8_t* from,
                                    const uint8_t* to,
                                    const uint8_t* amount) {
    if (simulator == NULL || token_address == NULL || 
        from == NULL || to == NULL || amount == NULL) {
        return -1;
    }
    
    if (simulator->callback == NULL) return -1;
    
    
    l2_chain_simulator_t* chain = NULL;
    for (size_t i = 0; i < simulator->chain_count; i++) {
        if (simulator->chains[i].chain_id == chain_id) {
            chain = &simulator->chains[i];
            break;
        }
    }
    
    if (chain == NULL) return -1;
    
    uint64_t tx_id = ++chain->tx_counter;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    uint64_t operation_id = tx_id * 2; 

    simulator->callback(chain_id, operation_id, tx_id, timestamp,
                        1, 
                        token_address, from, amount);
    
    
    simulator->callback(chain_id, operation_id + 1, tx_id, timestamp,
                        0, 
                        token_address, to, amount);
    
    chain->block_number++;
    
    return 0;
}

int l2_simulator_generate_mint(l2_simulator_manager_t* simulator,
                                 uint32_t chain_id,
                                 const uint8_t* token_address,
                                 const uint8_t* to,
                                 const uint8_t* amount) {
    if (simulator == NULL || token_address == NULL || 
        to == NULL || amount == NULL) {
        return -1;
    }
    
    if (simulator->callback == NULL) return -1;
    
    l2_chain_simulator_t* chain = NULL;
    for (size_t i = 0; i < simulator->chain_count; i++) {
        if (simulator->chains[i].chain_id == chain_id) {
            chain = &simulator->chains[i];
            break;
        }
    }
    
    if (chain == NULL) return -1;
    
    uint64_t tx_id = ++chain->tx_counter;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    uint64_t operation_id = tx_id * 2;

    simulator->callback(chain_id, operation_id, tx_id, timestamp,
                        0, 
                        token_address, to, amount);
    
    chain->block_number++;
    
    return 0;
}

int l2_simulator_generate_burn(l2_simulator_manager_t* simulator,
                                 uint32_t chain_id,
                                 const uint8_t* token_address,
                                 const uint8_t* from,
                                 const uint8_t* amount) {
    if (simulator == NULL || token_address == NULL || 
        from == NULL || amount == NULL) {
        return -1;
    }
    
    if (simulator->callback == NULL) return -1;
    
    l2_chain_simulator_t* chain = NULL;
    for (size_t i = 0; i < simulator->chain_count; i++) {
        if (simulator->chains[i].chain_id == chain_id) {
            chain = &simulator->chains[i];
            break;
        }
    }
    
    if (chain == NULL) return -1;
    
    uint64_t tx_id = ++chain->tx_counter;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    uint64_t operation_id = tx_id * 2;

    simulator->callback(chain_id, operation_id, tx_id, timestamp,
                        1, 
                        token_address, from, amount);
    
    chain->block_number++;
    
    return 0;
}

static void* simulator_thread_func(void* arg) {
    l2_simulator_manager_t* simulator = (l2_simulator_manager_t*)arg;

    uint8_t token1[42] = "0x1111111111111111111111111111111111111111";
    uint8_t token2[42] = "0x2222222222222222222222222222222222222222";
    uint8_t token3[42] = "0x3333333333333333333333333333333333333333";
    
    
    uint8_t account1[20], account2[20], account3[20];
    l2_simulator_generate_random_address(account1);
    l2_simulator_generate_random_address(account2);
    l2_simulator_generate_random_address(account3);
    
    uint8_t amount[32];
    
    while (g_thread_running && simulator->running) {
        
        if (simulator->chain_count == 0) {
            sleep(1);
            continue;
        }
        
        uint32_t chain_idx = rand() % simulator->chain_count;
        uint32_t chain_id = simulator->chains[chain_idx].chain_id;
        
        
        int tx_type = rand() % 3;
        
        
        uint8_t* token = NULL;
        uint8_t* from = NULL;
        uint8_t* to = NULL;
        
        switch (rand() % 3) {
            case 0: token = token1; break;
            case 1: token = token2; break;
            case 2: token = token3; break;
        }
        
        switch (rand() % 3) {
            case 0: from = account1; break;
            case 1: from = account2; break;
            case 2: from = account3; break;
        }
        
        switch (rand() % 3) {
            case 0: to = account1; break;
            case 1: to = account2; break;
            case 2: to = account3; break;
        }
        
        generate_random_amount(amount);
        
        
        switch (tx_type) {
            case 0: 
                l2_simulator_generate_transfer(simulator, chain_id, 
                                                token, from, to, amount);
                break;
            case 1: 
                l2_simulator_generate_mint(simulator, chain_id, 
                                            token, to, amount);
                break;
            case 2: 
                l2_simulator_generate_burn(simulator, chain_id, 
                                            token, from, amount);
                break;
        }
        
        
        usleep((rand() % 1000000) + 500000); 
    }

    return NULL;
}

int l2_simulator_start(l2_simulator_manager_t* simulator) {
    if (simulator == NULL) return -1;
    
    if (simulator->running) return 0; 
    
    if (simulator->chain_count == 0) {

        return -1;
    }
    
    if (simulator->callback == NULL) {

        return -1;
    }
    
    simulator->running = true;
    g_thread_running = true;
    
    if (pthread_create(&g_simulator_thread, NULL, simulator_thread_func, simulator) != 0) {
        simulator->running = false;
        g_thread_running = false;
        return -1;
    }

    return 0;
}

void l2_simulator_stop(l2_simulator_manager_t* simulator) {
    if (simulator == NULL) return;
    
    simulator->running = false;
    g_thread_running = false;
    
    if (g_simulator_thread) {
        pthread_join(g_simulator_thread, NULL);
    }

}
