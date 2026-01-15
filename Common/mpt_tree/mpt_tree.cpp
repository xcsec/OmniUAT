#include "mpt_tree.h"
#include "mpt_tree_common.h"
#include <string.h>
#include <stdlib.h>

static uint8_t nibble_to_hex(uint8_t nibble) {
    if (nibble < 10) return '0' + nibble;
    return 'a' + (nibble - 10);
}

static uint8_t hex_to_nibble(uint8_t hex) {
    if (hex >= '0' && hex <= '9') return hex - '0';
    if (hex >= 'a' && hex <= 'f') return hex - 'a' + 10;
    if (hex >= 'A' && hex <= 'F') return hex - 'A' + 10;
    return 0;
}

void mpt_node_hash(mpt_node_t* node, uint8_t* hash) {
    if (node == NULL || hash == NULL) return;
    
    uint8_t buffer[1024];
    size_t offset = 0;
    
    
    buffer[offset++] = (uint8_t)node->type;
    
    switch (node->type) {
        case MPT_NODE_LEAF:
            memcpy(buffer + offset, node->data.leaf.key, node->data.leaf.key_len);
            offset += node->data.leaf.key_len;
            memcpy(buffer + offset, node->data.leaf.value, node->data.leaf.value_len);
            offset += node->data.leaf.value_len;
            break;
            
        case MPT_NODE_EXTENSION:
            memcpy(buffer + offset, node->data.extension.key, node->data.extension.key_len);
            offset += node->data.extension.key_len;
            if (node->data.extension.next) {
                mpt_node_hash(node->data.extension.next, buffer + offset);
                offset += MPT_NODE_HASH_SIZE;
            }
            break;
            
        case MPT_NODE_BRANCH:
            for (int i = 0; i < 16; i++) {
                if (node->data.branch.children[i]) {
                    mpt_node_hash(node->data.branch.children[i], buffer + offset);
                    offset += MPT_NODE_HASH_SIZE;
                }
            }
            if (node->data.branch.value_len > 0) {
                memcpy(buffer + offset, node->data.branch.value, node->data.branch.value_len);
                offset += node->data.branch.value_len;
            }
            break;
            
        default:
            break;
    }
    
    
    platform_sha256(buffer, offset, hash);
    memcpy(node->hash, hash, MPT_NODE_HASH_SIZE);
}

static mpt_node_t* mpt_node_create(mpt_node_type_t type) {
    mpt_node_t* node = (mpt_node_t*)platform_malloc(sizeof(mpt_node_t));
    if (node == NULL) return NULL;
    
    memset(node, 0, sizeof(mpt_node_t));
    node->type = type;
    return node;
}

static void mpt_node_free(mpt_node_t* node) {
    if (node == NULL) return;
    
    switch (node->type) {
        case MPT_NODE_EXTENSION:
            if (node->data.extension.next) {
                mpt_node_free(node->data.extension.next);
            }
            break;
            
        case MPT_NODE_BRANCH:
            for (int i = 0; i < 16; i++) {
                if (node->data.branch.children[i]) {
                    mpt_node_free(node->data.branch.children[i]);
                }
            }
            break;
            
        default:
            break;
    }
    
    platform_free(node);
}

int mpt_tree_init(mpt_tree_t* tree) {
    if (tree == NULL) return -1;
    
    memset(tree, 0, sizeof(mpt_tree_t));
    tree->root = mpt_node_create(MPT_NODE_EMPTY);
    if (tree->root == NULL) return -1;
    
    memset(tree->root_hash, 0, MPT_NODE_HASH_SIZE);
    return 0;
}

void mpt_tree_destroy(mpt_tree_t* tree) {
    if (tree == NULL) return;
    
    if (tree->root) {
        mpt_node_free(tree->root);
        tree->root = NULL;
    }
    
    memset(tree, 0, sizeof(mpt_tree_t));
}

int mpt_tree_insert(mpt_tree_t* tree, const uint8_t* key, size_t key_len,
                     const uint8_t* value, size_t value_len) {
    if (tree == NULL || key == NULL || value == NULL) return -1;
    if (key_len > MPT_MAX_KEY_LEN || value_len > MPT_MAX_VALUE_LEN) return -1;
    
    
    mpt_node_t* leaf = mpt_node_create(MPT_NODE_LEAF);
    if (leaf == NULL) return -1;
    
    memcpy(leaf->data.leaf.key, key, key_len);
    leaf->data.leaf.key_len = key_len;
    memcpy(leaf->data.leaf.value, value, value_len);
    leaf->data.leaf.value_len = value_len;
    
    
    if (tree->root->type == MPT_NODE_EMPTY) {
        mpt_node_free(tree->root);
        tree->root = leaf;
    } else {
        
        
        mpt_node_free(tree->root);
        tree->root = leaf;
    }
    
    
    mpt_node_hash(tree->root, tree->root_hash);
    tree->size++;
    
    return 0;
}

int mpt_tree_get(mpt_tree_t* tree, const uint8_t* key, size_t key_len,
                  uint8_t* value, size_t* value_len) {
    if (tree == NULL || key == NULL || value == NULL || value_len == NULL) return -1;
    
    
    if (tree->root->type == MPT_NODE_LEAF) {
        if (tree->root->data.leaf.key_len == key_len &&
            memcmp(tree->root->data.leaf.key, key, key_len) == 0) {
            size_t copy_len = (*value_len < tree->root->data.leaf.value_len) ?
                              *value_len : tree->root->data.leaf.value_len;
            memcpy(value, tree->root->data.leaf.value, copy_len);
            *value_len = tree->root->data.leaf.value_len;
            return 0;
        }
    }
    
    return -1; 
}

int mpt_tree_delete(mpt_tree_t* tree, const uint8_t* key, size_t key_len) {
    if (tree == NULL || key == NULL) return -1;
    
    
    if (tree->root->type == MPT_NODE_LEAF) {
        if (tree->root->data.leaf.key_len == key_len &&
            memcmp(tree->root->data.leaf.key, key, key_len) == 0) {
            mpt_node_free(tree->root);
            tree->root = mpt_node_create(MPT_NODE_EMPTY);
            if (tree->root == NULL) return -1;
            
            memset(tree->root_hash, 0, MPT_NODE_HASH_SIZE);
            tree->size--;
            return 0;
        }
    }
    
    return -1; 
}

int mpt_tree_get_root_hash(mpt_tree_t* tree, uint8_t* root_hash) {
    if (tree == NULL || root_hash == NULL) return -1;
    
    memcpy(root_hash, tree->root_hash, MPT_NODE_HASH_SIZE);
    return 0;
}
