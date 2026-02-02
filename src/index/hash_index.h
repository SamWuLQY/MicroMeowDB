#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <stdint.h>
#include <stdbool.h>

// 哈希桶大小
#define HASH_INDEX_BUCKET_SIZE 1024

// 哈希节点结构
typedef struct hash_node {
    char *key;
    uint32_t key_size;
    char *value;
    uint32_t value_size;
    struct hash_node *next;
} hash_node;

// 哈希索引结构
typedef struct {
    hash_node **buckets;
    uint32_t bucket_count;
    uint32_t item_count;
} hash_index;

// 初始化哈希索引
hash_index *hash_index_create(uint32_t bucket_count);

// 销毁哈希索引
void hash_index_destroy(hash_index *index);

// 插入键值对
bool hash_index_insert(hash_index *index, const char *key, uint32_t key_size, const char *value, uint32_t value_size);

// 查询键值对
char *hash_index_get(hash_index *index, const char *key, uint32_t key_size, uint32_t *value_size);

// 删除键值对
bool hash_index_delete(hash_index *index, const char *key, uint32_t key_size);

// 获取哈希索引大小
uint32_t hash_index_size(hash_index *index);

#endif // HASH_INDEX_H
