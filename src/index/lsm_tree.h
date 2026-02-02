#ifndef LSM_TREE_H
#define LSM_TREE_H

#include <stdint.h>
#include <stdbool.h>

// LSM树配置参数
#define LSM_MEMTABLE_MAX_SIZE (1024 * 1024 * 10) // 10MB
#define LSM_SSTABLE_LEVELS 3
#define LSM_SSTABLE_RATIO 10

// 键值对结构
typedef struct {
    uint32_t key_size;
    uint32_t value_size;
    char *key;
    char *value;
} lsm_kv_pair;

// 内存表结构 (MemTable)
typedef struct {
    char *data;
    uint32_t size;
    uint32_t capacity;
    bool immutable;
} lsm_memtable;

// SSTable文件元数据
typedef struct {
    char *filename;
    uint64_t min_key;
    uint64_t max_key;
    uint32_t entry_count;
    uint32_t level;
} lsm_sstable_meta;

// LSM树结构
typedef struct {
    lsm_memtable *active_memtable;
    lsm_memtable *immutable_memtable;
    lsm_sstable_meta **sstables[LSM_SSTABLE_LEVELS];
    uint32_t sstable_counts[LSM_SSTABLE_LEVELS];
    char *base_dir;
} lsm_tree;

// 初始化LSM树
lsm_tree *lsm_tree_create(const char *base_dir);

// 销毁LSM树
void lsm_tree_destroy(lsm_tree *tree);

// 插入键值对
bool lsm_tree_insert(lsm_tree *tree, const char *key, uint32_t key_size, const char *value, uint32_t value_size);

// 查询键值对
char *lsm_tree_get(lsm_tree *tree, const char *key, uint32_t key_size, uint32_t *value_size);

// 删除键值对
bool lsm_tree_delete(lsm_tree *tree, const char *key, uint32_t key_size);

// 强制刷写内存表到磁盘
bool lsm_tree_flush(lsm_tree *tree);

// 执行后台压缩
bool lsm_tree_compact(lsm_tree *tree);

#endif // LSM_TREE_H
