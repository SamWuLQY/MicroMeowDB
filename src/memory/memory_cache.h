#ifndef MEMORY_CACHE_H
#define MEMORY_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include "memory_pool.h"

// 前向声明
struct config_system;

// 缓存项结构
typedef struct {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    uint32_t access_count; // 访问计数
    uint64_t last_access_time; // 最后访问时间
    bool in_use;
    struct CacheItem* next;
    struct CacheItem* prev;
} CacheItem;

// 内存缓存结构
typedef struct {
    MemoryPool* memory_pool;
    CacheItem* items;
    size_t capacity; // 缓存容量（字节）
    size_t used; // 已使用容量
    size_t item_count; // 缓存项数量
    size_t max_item_count; // 最大缓存项数量
    CacheItem* head; // LRU链表头
    CacheItem* tail; // LRU链表尾
    uint64_t current_time; // 当前时间戳
} MemoryCache;

// 缓存配置
#define DEFAULT_CACHE_CAPACITY (1024 * 1024 * 1024) // 1GB
#define DEFAULT_MAX_ITEM_COUNT 100000
#define CACHE_ITEM_OVERHEAD sizeof(CacheItem)

// 初始化内存缓存
MemoryCache* memory_cache_init(struct config_system *config, MemoryPool* pool);

// 添加或更新缓存项
bool memory_cache_set(MemoryCache* cache, void* key, size_t key_size, void* value, size_t value_size);

// 获取缓存项
void* memory_cache_get(MemoryCache* cache, void* key, size_t key_size, size_t* value_size);

// 删除缓存项
bool memory_cache_delete(MemoryCache* cache, void* key, size_t key_size);

// 清空缓存
void memory_cache_clear(MemoryCache* cache);

// 获取缓存状态
void memory_cache_status(MemoryCache* cache);

// 销毁缓存
void memory_cache_destroy(MemoryCache* cache);

// 缓存淘汰策略
static bool memory_cache_evict(MemoryCache* cache);

// 查找缓存项
static CacheItem* memory_cache_find(MemoryCache* cache, void* key, size_t key_size);

// 更新缓存项访问信息
static void memory_cache_update_access(MemoryCache* cache, CacheItem* item);

// 将缓存项移到链表头部
static void memory_cache_move_to_head(MemoryCache* cache, CacheItem* item);

// 计算缓存项大小
static size_t memory_cache_item_size(size_t key_size, size_t value_size);

#endif // MEMORY_CACHE_H