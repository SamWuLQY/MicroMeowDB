#include "hash_index.h"
#include <stdlib.h>
#include <string.h>

// 哈希函数
static uint32_t hash_function(const char *key, uint32_t key_size, uint32_t bucket_count) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < key_size; i++) {
        hash = ((hash << 5) + hash) + key[i];
    }
    return hash % bucket_count;
}

// 创建哈希节点
static hash_node *hash_node_create(const char *key, uint32_t key_size, const char *value, uint32_t value_size) {
    hash_node *node = (hash_node *)malloc(sizeof(hash_node));
    if (!node) {
        return NULL;
    }
    
    node->key = (char *)malloc(key_size);
    if (!node->key) {
        free(node);
        return NULL;
    }
    memcpy(node->key, key, key_size);
    node->key_size = key_size;
    
    node->value = (char *)malloc(value_size);
    if (!node->value) {
        free(node->key);
        free(node);
        return NULL;
    }
    memcpy(node->value, value, value_size);
    node->value_size = value_size;
    
    node->next = NULL;
    return node;
}

// 销毁哈希节点
static void hash_node_destroy(hash_node *node) {
    if (node) {
        if (node->key) {
            free(node->key);
        }
        if (node->value) {
            free(node->value);
        }
        free(node);
    }
}

// 初始化哈希索引
hash_index *hash_index_create(uint32_t bucket_count) {
    if (bucket_count == 0) {
        bucket_count = HASH_INDEX_BUCKET_SIZE;
    }
    
    hash_index *index = (hash_index *)malloc(sizeof(hash_index));
    if (!index) {
        return NULL;
    }
    
    index->buckets = (hash_node **)calloc(bucket_count, sizeof(hash_node *));
    if (!index->buckets) {
        free(index);
        return NULL;
    }
    
    index->bucket_count = bucket_count;
    index->item_count = 0;
    
    return index;
}

// 销毁哈希索引
void hash_index_destroy(hash_index *index) {
    if (index) {
        for (uint32_t i = 0; i < index->bucket_count; i++) {
            hash_node *node = index->buckets[i];
            while (node) {
                hash_node *next = node->next;
                hash_node_destroy(node);
                node = next;
            }
        }
        free(index->buckets);
        free(index);
    }
}

// 插入键值对
bool hash_index_insert(hash_index *index, const char *key, uint32_t key_size, const char *value, uint32_t value_size) {
    if (!index || !key) {
        return false;
    }
    
    // 计算哈希值
    uint32_t bucket_idx = hash_function(key, key_size, index->bucket_count);
    
    // 检查是否已存在
    hash_node *current = index->buckets[bucket_idx];
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            // 更新现有值
            char *new_value = (char *)realloc(current->value, value_size);
            if (!new_value) {
                return false;
            }
            memcpy(new_value, value, value_size);
            current->value = new_value;
            current->value_size = value_size;
            return true;
        }
        current = current->next;
    }
    
    // 创建新节点
    hash_node *node = hash_node_create(key, key_size, value, value_size);
    if (!node) {
        return false;
    }
    
    // 插入到链表头部
    node->next = index->buckets[bucket_idx];
    index->buckets[bucket_idx] = node;
    index->item_count++;
    
    return true;
}

// 查询键值对
char *hash_index_get(hash_index *index, const char *key, uint32_t key_size, uint32_t *value_size) {
    if (!index || !key) {
        return NULL;
    }
    
    // 计算哈希值
    uint32_t bucket_idx = hash_function(key, key_size, index->bucket_count);
    
    // 查找节点
    hash_node *current = index->buckets[bucket_idx];
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            *value_size = current->value_size;
            char *value = (char *)malloc(current->value_size);
            if (value) {
                memcpy(value, current->value, current->value_size);
            }
            return value;
        }
        current = current->next;
    }
    
    return NULL;
}

// 删除键值对
bool hash_index_delete(hash_index *index, const char *key, uint32_t key_size) {
    if (!index || !key) {
        return false;
    }
    
    // 计算哈希值
    uint32_t bucket_idx = hash_function(key, key_size, index->bucket_count);
    
    // 查找并删除节点
    hash_node *current = index->buckets[bucket_idx];
    hash_node *prev = NULL;
    
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                index->buckets[bucket_idx] = current->next;
            }
            hash_node_destroy(current);
            index->item_count--;
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    return false;
}

// 获取哈希索引大小
uint32_t hash_index_size(hash_index *index) {
    if (!index) {
        return 0;
    }
    return index->item_count;
}
