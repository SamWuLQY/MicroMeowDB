#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <stdint.h>
#include <stdbool.h>

// 布隆过滤器配置参数
#define BLOOM_FILTER_DEFAULT_SIZE (1024 * 1024 * 8) // 1MB
#define BLOOM_FILTER_DEFAULT_HASHES 3

// 布隆过滤器结构
typedef struct {
    uint8_t *bits;
    uint32_t size;
    uint32_t hash_count;
    uint32_t item_count;
} bloom_filter;

// 初始化布隆过滤器
bloom_filter *bloom_filter_create(uint32_t size, uint32_t hash_count);

// 销毁布隆过滤器
void bloom_filter_destroy(bloom_filter *filter);

// 添加元素到布隆过滤器
bool bloom_filter_add(bloom_filter *filter, const char *key, uint32_t key_size);

// 检查元素是否可能在布隆过滤器中
bool bloom_filter_contains(bloom_filter *filter, const char *key, uint32_t key_size);

// 重置布隆过滤器
bool bloom_filter_reset(bloom_filter *filter);

// 获取布隆过滤器大小
uint32_t bloom_filter_size(bloom_filter *filter);

// 获取布隆过滤器中元素数量
uint32_t bloom_filter_item_count(bloom_filter *filter);

#endif // BLOOM_FILTER_H
