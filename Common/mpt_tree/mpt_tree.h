#ifndef _MPT_TREE_H_
#define _MPT_TREE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MPT_NODE_HASH_SIZE 32
#define MPT_MAX_KEY_LEN 64
#define MPT_MAX_VALUE_LEN 256

typedef enum {
    MPT_NODE_EMPTY = 0,
    MPT_NODE_LEAF = 1,
    MPT_NODE_EXTENSION = 2,
    MPT_NODE_BRANCH = 3
} mpt_node_type_t;

typedef struct mpt_node {
    mpt_node_type_t type;
    uint8_t hash[MPT_NODE_HASH_SIZE];  
    union {
        
        struct {
            uint8_t key[MPT_MAX_KEY_LEN];
            size_t key_len;
            uint8_t value[MPT_MAX_VALUE_LEN];
            size_t value_len;
        } leaf;
        
        
        struct {
            uint8_t key[MPT_MAX_KEY_LEN];
            size_t key_len;
            struct mpt_node* next;
        } extension;
        
        
        struct {
            struct mpt_node* children[16];
            uint8_t value[MPT_MAX_VALUE_LEN];
            size_t value_len;
        } branch;
    } data;
} mpt_node_t;

typedef struct {
    mpt_node_t* root;
    uint8_t root_hash[MPT_NODE_HASH_SIZE];
    size_t size;  
} mpt_tree_t;

int mpt_tree_init(mpt_tree_t* tree);

void mpt_tree_destroy(mpt_tree_t* tree);

int mpt_tree_insert(mpt_tree_t* tree, const uint8_t* key, size_t key_len,
                     const uint8_t* value, size_t value_len);

int mpt_tree_get(mpt_tree_t* tree, const uint8_t* key, size_t key_len,
                  uint8_t* value, size_t* value_len);

int mpt_tree_delete(mpt_tree_t* tree, const uint8_t* key, size_t key_len);

int mpt_tree_get_root_hash(mpt_tree_t* tree, uint8_t* root_hash);

void mpt_node_hash(mpt_node_t* node, uint8_t* hash);

#endif
