#ifndef BITMAP_INDEX_H
#define BITMAP_INDEX_H

#include <stdint.h>
#include <stdbool.h>

// 位图块大小
#define BITMAP_INDEX_BLOCK_SIZE 64

// 位图结构
typedef struct {
    uint64_t *blocks;
    uint32_t block_count;
    uint32_t bit_count;
} bitmap;

// 位图索引项结构
typedef struct {
    char *value;
    uint32_t value_size;
    bitmap *bits;
} bitmap_index_item;

// 位图索引结构
typedef struct {
    bitmap_index_item **items;
    uint32_t item_count;
    uint32_t capacity;
    uint32_t max_bit_position;
} bitmap_index;

// 初始化位图
bitmap *bitmap_create(uint32_t size);

// 销毁位图
void bitmap_destroy(bitmap *bitmap);

// 设置位图中的位
bool bitmap_set(bitmap *bitmap, uint32_t position);

// 清除位图中的位
bool bitmap_clear(bitmap *bitmap, uint32_t position);

// 检查位图中的位是否设置
bool bitmap_test(bitmap *bitmap, uint32_t position);

// 位图与操作
bitmap *bitmap_and(const bitmap *a, const bitmap *b);

// 位图或操作
bitmap *bitmap_or(const bitmap *a, const bitmap *b);

// 位图非操作
bitmap *bitmap_not(const bitmap *bitmap);

// 初始化位图索引
bitmap_index *bitmap_index_create(uint32_t capacity);

// 销毁位图索引
void bitmap_index_destroy(bitmap_index *index);

// 插入数据到位图索引
bool bitmap_index_insert(bitmap_index *index, const char *value, uint32_t value_size, uint32_t row_id);

// 查询特定值的位图
bitmap *bitmap_index_query(bitmap_index *index, const char *value, uint32_t value_size);

// 获取位图索引中的所有值
char **bitmap_index_get_values(bitmap_index *index, uint32_t *value_counts, uint32_t *result_count);

// 获取位图索引大小
uint32_t bitmap_index_size(bitmap_index *index);

#endif // BITMAP_INDEX_H
