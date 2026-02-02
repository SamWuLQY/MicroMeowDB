#include "memory_cache.h"
#include "../config/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// 计算缓存项大小
static size_t memory_cache_item_size(size_t key_size, size_t value_size) {
    return CACHE_ITEM_OVERHEAD + key_size + value_size;
}

// 查找缓存项
static CacheItem* memory_cache_find(MemoryCache* cache, void* key, size_t key_size) {
    if (!cache || !key || key_size == 0) {
        return NULL;
    }

    // 遍历缓存项查找
    // 实际实现中应该使用哈希表来加速查找
    CacheItem* current = cache->head;
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// 更新缓存项访问信息
static void memory_cache_update_access(MemoryCache* cache, CacheItem* item) {
    if (!cache || !item) {
        return;
    }

    // 更新访问计数和时间
    item->access_count++;
    item->last_access_time = cache->current_time;

    // 将项移到链表头部
    memory_cache_move_to_head(cache, item);
}

// 将缓存项移到链表头部
static void memory_cache_move_to_head(MemoryCache* cache, CacheItem* item) {
    if (!cache || !item || item == cache->head) {
        return;
    }

    // 从当前位置移除
    if (item->prev) {
        item->prev->next = item->next;
    }
    if (item->next) {
        item->next->prev = item->prev;
    }
    if (item == cache->tail) {
        cache->tail = item->prev;
    }

    // 添加到头部
    item->next = cache->head;
    item->prev = NULL;
    if (cache->head) {
        cache->head->prev = item;
    }
    cache->head = item;
    if (!cache->tail) {
        cache->tail = item;
    }
}

// 缓存淘汰策略
static bool memory_cache_evict(MemoryCache* cache) {
    if (!cache || cache->item_count == 0) {
        return false;
    }

    // 使用LRU+LFU混合策略
    // 首先尝试淘汰访问计数最低的项
    // 如果计数相同，淘汰最后访问时间最早的项

    CacheItem* evict_item = NULL;
    CacheItem* current = cache->tail;

    // 从链表尾部开始查找（LRU顺序）
    while (current) {
        if (!evict_item || current->access_count < evict_item->access_count ||
            (current->access_count == evict_item->access_count && 
             current->last_access_time < evict_item->last_access_time)) {
            evict_item = current;
        }
        current = current->prev;
    }

    if (!evict_item) {
        return false;
    }

    // 计算要释放的空间
    size_t item_size = memory_cache_item_size(evict_item->key_size, evict_item->value_size);

    // 从链表中移除
    if (evict_item->prev) {
        evict_item->prev->next = evict_item->next;
    }
    if (evict_item->next) {
        evict_item->next->prev = evict_item->prev;
    }
    if (evict_item == cache->head) {
        cache->head = evict_item->next;
    }
    if (evict_item == cache->tail) {
        cache->tail = evict_item->prev;
    }

    // 释放内存
    if (evict_item->key) {
        memory_pool_free(cache->memory_pool, evict_item->key);
    }
    if (evict_item->value) {
        memory_pool_free(cache->memory_pool, evict_item->value);
    }
    memory_pool_free(cache->memory_pool, evict_item);

    // 更新缓存统计
    cache->used -= item_size;
    cache->item_count--;

    return true;
}

// 初始化内存缓存
MemoryCache* memory_cache_init(config_system *config, MemoryPool* pool) {
    if (!pool) {
        return NULL;
    }

    // 从配置系统获取缓存大小，默认256MB
    size_t capacity = config ? config_get_int(config, "memory.cache_size", 256) * 1024 * 1024 : DEFAULT_CACHE_CAPACITY;
    
    // 从配置系统获取最大项数，默认100000
    size_t max_item_count = config ? config_get_int(config, "memory.cache_max_items", 100000) : DEFAULT_MAX_ITEM_COUNT;

    MemoryCache* cache = (MemoryCache*)memory_pool_alloc(pool, sizeof(MemoryCache));
    if (!cache) {
        fprintf(stderr, "Failed to allocate memory for cache\n");
        return NULL;
    }

    cache->memory_pool = pool;
    cache->capacity = capacity > 0 ? capacity : DEFAULT_CACHE_CAPACITY;
    cache->max_item_count = max_item_count > 0 ? max_item_count : DEFAULT_MAX_ITEM_COUNT;
    cache->used = 0;
    cache->item_count = 0;
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_time = time(NULL);

    return cache;
}

// 添加或更新缓存项
bool memory_cache_set(MemoryCache* cache, void* key, size_t key_size, void* value, size_t value_size) {
    if (!cache || !key || key_size == 0 || !value || value_size == 0) {
        return false;
    }

    // 计算缓存项大小
    size_t item_size = memory_cache_item_size(key_size, value_size);

    // 检查是否需要淘汰
    while (cache->used + item_size > cache->capacity || cache->item_count >= cache->max_item_count) {
        if (!memory_cache_evict(cache)) {
            return false;
        }
    }

    // 查找是否已存在
    CacheItem* existing_item = memory_cache_find(cache, key, key_size);
    if (existing_item) {
        // 更新现有项
        size_t old_size = memory_cache_item_size(existing_item->key_size, existing_item->value_size);

        // 释放旧值
        if (existing_item->value) {
            memory_pool_free(cache->memory_pool, existing_item->value);
        }

        // 分配新值
        void* new_value = memory_pool_alloc(cache->memory_pool, value_size);
        if (!new_value) {
            return false;
        }
        memcpy(new_value, value, value_size);

        // 更新项信息
        existing_item->value = new_value;
        existing_item->value_size = value_size;

        // 更新大小统计
        cache->used = cache->used - old_size + item_size;

        // 更新访问信息
        memory_cache_update_access(cache, existing_item);

        return true;
    }

    // 创建新项
    CacheItem* new_item = (CacheItem*)memory_pool_alloc(cache->memory_pool, sizeof(CacheItem));
    if (!new_item) {
        return false;
    }

    // 分配键和值
    void* new_key = memory_pool_alloc(cache->memory_pool, key_size);
    if (!new_key) {
        memory_pool_free(cache->memory_pool, new_item);
        return false;
    }

    void* new_value = memory_pool_alloc(cache->memory_pool, value_size);
    if (!new_value) {
        memory_pool_free(cache->memory_pool, new_key);
        memory_pool_free(cache->memory_pool, new_item);
        return false;
    }

    // 复制数据
    memcpy(new_key, key, key_size);
    memcpy(new_value, value, value_size);

    // 初始化新项
    new_item->key = new_key;
    new_item->key_size = key_size;
    new_item->value = new_value;
    new_item->value_size = value_size;
    new_item->access_count = 1;
    new_item->last_access_time = cache->current_time;
    new_item->in_use = true;
    new_item->next = NULL;
    new_item->prev = NULL;

    // 添加到链表头部
    if (!cache->head) {
        cache->head = new_item;
        cache->tail = new_item;
    } else {
        new_item->next = cache->head;
        cache->head->prev = new_item;
        cache->head = new_item;
    }

    // 更新统计
    cache->used += item_size;
    cache->item_count++;

    return true;
}

// 获取缓存项
void* memory_cache_get(MemoryCache* cache, void* key, size_t key_size, size_t* value_size) {
    if (!cache || !key || key_size == 0) {
        return NULL;
    }

    // 更新当前时间
    cache->current_time = time(NULL);

    // 查找缓存项
    CacheItem* item = memory_cache_find(cache, key, key_size);
    if (!item) {
        return NULL;
    }

    // 更新访问信息
    memory_cache_update_access(cache, item);

    // 设置返回值大小
    if (value_size) {
        *value_size = item->value_size;
    }

    // 返回值的副本
    void* value_copy = memory_pool_alloc(cache->memory_pool, item->value_size);
    if (!value_copy) {
        return NULL;
    }
    memcpy(value_copy, item->value, item->value_size);

    return value_copy;
}

// 删除缓存项
bool memory_cache_delete(MemoryCache* cache, void* key, size_t key_size) {
    if (!cache || !key || key_size == 0) {
        return false;
    }

    // 查找缓存项
    CacheItem* item = memory_cache_find(cache, key, key_size);
    if (!item) {
        return false;
    }

    // 计算项大小
    size_t item_size = memory_cache_item_size(item->key_size, item->value_size);

    // 从链表中移除
    if (item->prev) {
        item->prev->next = item->next;
    }
    if (item->next) {
        item->next->prev = item->prev;
    }
    if (item == cache->head) {
        cache->head = item->next;
    }
    if (item == cache->tail) {
        cache->tail = item->prev;
    }

    // 释放内存
    if (item->key) {
        memory_pool_free(cache->memory_pool, item->key);
    }
    if (item->value) {
        memory_pool_free(cache->memory_pool, item->value);
    }
    memory_pool_free(cache->memory_pool, item);

    // 更新统计
    cache->used -= item_size;
    cache->item_count--;

    return true;
}

// 清空缓存
void memory_cache_clear(MemoryCache* cache) {
    if (!cache) {
        return;
    }

    // 遍历释放所有项
    CacheItem* current = cache->head;
    while (current) {
        CacheItem* next = current->next;

        // 释放内存
        if (current->key) {
            memory_pool_free(cache->memory_pool, current->key);
        }
        if (current->value) {
            memory_pool_free(cache->memory_pool, current->value);
        }
        memory_pool_free(cache->memory_pool, current);

        current = next;
    }

    // 重置缓存状态
    cache->head = NULL;
    cache->tail = NULL;
    cache->used = 0;
    cache->item_count = 0;
}

// 获取缓存状态
void memory_cache_status(MemoryCache* cache) {
    if (!cache) {
        return;
    }

    printf("Memory Cache Status:\n");
    printf("Capacity: %zu bytes\n", cache->capacity);
    printf("Used: %zu bytes\n", cache->used);
    printf("Free: %zu bytes\n", cache->capacity - cache->used);
    printf("Item Count: %zu\n", cache->item_count);
    printf("Max Item Count: %zu\n", cache->max_item_count);

    // 统计访问计数分布
    size_t count_1 = 0, count_2_5 = 0, count_6_10 = 0, count_11_plus = 0;
    CacheItem* current = cache->head;
    while (current) {
        if (current->access_count == 1) {
            count_1++;
        } else if (current->access_count >= 2 && current->access_count <= 5) {
            count_2_5++;
        } else if (current->access_count >= 6 && current->access_count <= 10) {
            count_6_10++;
        } else {
            count_11_plus++;
        }
        current = current->next;
    }

    printf("Access Count Distribution:\n");
    printf("  1: %zu\n", count_1);
    printf("  2-5: %zu\n", count_2_5);
    printf("  6-10: %zu\n", count_6_10);
    printf("  11+: %zu\n", count_11_plus);
}

// 销毁缓存
void memory_cache_destroy(MemoryCache* cache) {
    if (!cache) {
        return;
    }

    // 清空缓存
    memory_cache_clear(cache);

    // 释放缓存结构
    memory_pool_free(cache->memory_pool, cache);
}