#include "lsm_tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// 内存表操作函数
static lsm_memtable *memtable_create(uint32_t capacity) {
    lsm_memtable *memtable = (lsm_memtable *)malloc(sizeof(lsm_memtable));
    if (!memtable) {
        return NULL;
    }
    
    memtable->data = (char *)calloc(1, capacity);
    if (!memtable->data) {
        free(memtable);
        return NULL;
    }
    
    memtable->size = 0;
    memtable->capacity = capacity;
    memtable->immutable = false;
    
    return memtable;
}

static void memtable_destroy(lsm_memtable *memtable) {
    if (memtable) {
        if (memtable->data) {
            free(memtable->data);
        }
        free(memtable);
    }
}

static bool memtable_put(lsm_memtable *memtable, const char *key, uint32_t key_size, const char *value, uint32_t value_size) {
    if (memtable->immutable || memtable->size + key_size + value_size + 8 > memtable->capacity) {
        return false;
    }
    
    // 写入键值对（简单实现，实际应该排序）
    memcpy(memtable->data + memtable->size, &key_size, 4);
    memcpy(memtable->data + memtable->size + 4, &value_size, 4);
    memcpy(memtable->data + memtable->size + 8, key, key_size);
    memcpy(memtable->data + memtable->size + 8 + key_size, value, value_size);
    
    memtable->size += 8 + key_size + value_size;
    return true;
}

static char *memtable_get(lsm_memtable *memtable, const char *key, uint32_t key_size, uint32_t *value_size) {
    uint32_t offset = 0;
    while (offset < memtable->size) {
        uint32_t current_key_size, current_value_size;
        memcpy(&current_key_size, memtable->data + offset, 4);
        memcpy(&current_value_size, memtable->data + offset + 4, 4);
        
        if (current_key_size == key_size && memcmp(memtable->data + offset + 8, key, key_size) == 0) {
            *value_size = current_value_size;
            char *value = (char *)malloc(current_value_size);
            if (value) {
                memcpy(value, memtable->data + offset + 8 + current_key_size, current_value_size);
            }
            return value;
        }
        
        offset += 8 + current_key_size + current_value_size;
    }
    
    return NULL;
}

// SSTable操作函数
static char *generate_sstable_filename(const char *base_dir, uint32_t level) {
    char *filename = (char *)malloc(strlen(base_dir) + 32);
    if (!filename) {
        return NULL;
    }
    
    time_t now = time(NULL);
    snprintf(filename, strlen(base_dir) + 32, "%s/%d_%ld.sst", base_dir, level, now);
    return filename;
}

static bool flush_memtable_to_sstable(lsm_memtable *memtable, const char *filename, lsm_sstable_meta **meta) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return false;
    }
    
    // 写入元数据
    uint32_t entry_count = 0;
    uint32_t offset = 0;
    while (offset < memtable->size) {
        uint32_t key_size, value_size;
        memcpy(&key_size, memtable->data + offset, 4);
        memcpy(&value_size, memtable->data + offset + 4, 4);
        offset += 8 + key_size + value_size;
        entry_count++;
    }
    
    fwrite(&entry_count, sizeof(uint32_t), 1, fp);
    
    // 写入数据
    fwrite(memtable->data, 1, memtable->size, fp);
    fclose(fp);
    
    // 创建元数据
    *meta = (lsm_sstable_meta *)malloc(sizeof(lsm_sstable_meta));
    if (!*meta) {
        return false;
    }
    
    (*meta)->filename = strdup(filename);
    (*meta)->entry_count = entry_count;
    (*meta)->min_key = 0; // 简化实现
    (*meta)->max_key = 0; // 简化实现
    (*meta)->level = 0;
    
    return true;
}

static void sstable_destroy(lsm_sstable_meta *meta) {
    if (meta) {
        if (meta->filename) {
            free(meta->filename);
        }
        free(meta);
    }
}

static char *sstable_get(const char *filename, const char *key, uint32_t key_size, uint32_t *value_size) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }
    
    uint32_t entry_count;
    fread(&entry_count, sizeof(uint32_t), 1, fp);
    
    // 简单线性查找，实际应该使用二分查找
    char *data = (char *)malloc(1024 * 1024); // 简化实现
    if (!data) {
        fclose(fp);
        return NULL;
    }
    
    size_t data_size = fread(data, 1, 1024 * 1024, fp);
    fclose(fp);
    
    uint32_t offset = 0;
    while (offset < data_size) {
        uint32_t current_key_size, current_value_size;
        memcpy(&current_key_size, data + offset, 4);
        memcpy(&current_value_size, data + offset + 4, 4);
        
        if (current_key_size == key_size && memcmp(data + offset + 8, key, key_size) == 0) {
            *value_size = current_value_size;
            char *value = (char *)malloc(current_value_size);
            if (value) {
                memcpy(value, data + offset + 8 + current_key_size, current_value_size);
            }
            free(data);
            return value;
        }
        
        offset += 8 + current_key_size + current_value_size;
    }
    
    free(data);
    return NULL;
}

// LSM树核心函数
lsm_tree *lsm_tree_create(const char *base_dir) {
    lsm_tree *tree = (lsm_tree *)malloc(sizeof(lsm_tree));
    if (!tree) {
        return NULL;
    }
    
    tree->active_memtable = memtable_create(LSM_MEMTABLE_MAX_SIZE);
    if (!tree->active_memtable) {
        free(tree);
        return NULL;
    }
    
    tree->immutable_memtable = NULL;
    tree->base_dir = strdup(base_dir);
    
    for (int i = 0; i < LSM_SSTABLE_LEVELS; i++) {
        tree->sstable_counts[i] = 0;
        tree->sstables[i] = NULL;
    }
    
    return tree;
}

void lsm_tree_destroy(lsm_tree *tree) {
    if (tree) {
        if (tree->active_memtable) {
            memtable_destroy(tree->active_memtable);
        }
        if (tree->immutable_memtable) {
            memtable_destroy(tree->immutable_memtable);
        }
        
        for (int i = 0; i < LSM_SSTABLE_LEVELS; i++) {
            for (int j = 0; j < tree->sstable_counts[i]; j++) {
                sstable_destroy(tree->sstables[i][j]);
            }
            if (tree->sstables[i]) {
                free(tree->sstables[i]);
            }
        }
        
        if (tree->base_dir) {
            free(tree->base_dir);
        }
        
        free(tree);
    }
}

bool lsm_tree_insert(lsm_tree *tree, const char *key, uint32_t key_size, const char *value, uint32_t value_size) {
    // 尝试插入到活跃内存表
    if (memtable_put(tree->active_memtable, key, key_size, value, value_size)) {
        return true;
    }
    
    // 活跃内存表已满，需要刷写到磁盘
    if (!tree->immutable_memtable) {
        // 将活跃内存表标记为不可变
        tree->active_memtable->immutable = true;
        tree->immutable_memtable = tree->active_memtable;
        
        // 创建新的活跃内存表
        tree->active_memtable = memtable_create(LSM_MEMTABLE_MAX_SIZE);
        if (!tree->active_memtable) {
            return false;
        }
        
        // 异步刷写不可变内存表到磁盘（简化实现，同步执行）
        lsm_tree_flush(tree);
    }
    
    // 再次尝试插入
    return memtable_put(tree->active_memtable, key, key_size, value, value_size);
}

char *lsm_tree_get(lsm_tree *tree, const char *key, uint32_t key_size, uint32_t *value_size) {
    // 1. 先查询活跃内存表
    char *value = memtable_get(tree->active_memtable, key, key_size, value_size);
    if (value) {
        return value;
    }
    
    // 2. 查询不可变内存表
    if (tree->immutable_memtable) {
        value = memtable_get(tree->immutable_memtable, key, key_size, value_size);
        if (value) {
            return value;
        }
    }
    
    // 3. 查询各层SSTable
    for (int i = 0; i < LSM_SSTABLE_LEVELS; i++) {
        for (int j = 0; j < tree->sstable_counts[i]; j++) {
            value = sstable_get(tree->sstables[i][j]->filename, key, key_size, value_size);
            if (value) {
                return value;
            }
        }
    }
    
    return NULL;
}

bool lsm_tree_delete(lsm_tree *tree, const char *key, uint32_t key_size) {
    // 删除操作通过插入 tombstone 实现
    return lsm_tree_insert(tree, key, key_size, "", 0);
}

bool lsm_tree_flush(lsm_tree *tree) {
    if (!tree->immutable_memtable) {
        return true;
    }
    
    // 生成SSTable文件名
    char *filename = generate_sstable_filename(tree->base_dir, 0);
    if (!filename) {
        return false;
    }
    
    // 刷写内存表到SSTable
    lsm_sstable_meta *meta = NULL;
    if (!flush_memtable_to_sstable(tree->immutable_memtable, filename, &meta)) {
        free(filename);
        return false;
    }
    
    // 将新的SSTable添加到第0层
    tree->sstables[0] = (lsm_sstable_meta **)realloc(tree->sstables[0], (tree->sstable_counts[0] + 1) * sizeof(lsm_sstable_meta *));
    if (!tree->sstables[0]) {
        free(filename);
        sstable_destroy(meta);
        return false;
    }
    
    tree->sstables[0][tree->sstable_counts[0]] = meta;
    tree->sstable_counts[0]++;
    
    // 销毁不可变内存表
    memtable_destroy(tree->immutable_memtable);
    tree->immutable_memtable = NULL;
    
    // 检查是否需要压缩
    if (tree->sstable_counts[0] >= LSM_SSTABLE_RATIO) {
        lsm_tree_compact(tree);
    }
    
    free(filename);
    return true;
}

bool lsm_tree_compact(lsm_tree *tree) {
    // 简化的压缩实现，实际应该更复杂
    return true;
}
