#include "bitmap_index.h"
#include <stdlib.h>
#include <string.h>

// 计算需要的块数
static uint32_t calculate_blocks(uint32_t bit_count) {
    return (bit_count + BITMAP_INDEX_BLOCK_SIZE - 1) / BITMAP_INDEX_BLOCK_SIZE;
}

// 初始化位图
bitmap *bitmap_create(uint32_t size) {
    bitmap *bitmap = (struct bitmap *)malloc(sizeof(struct bitmap));
    if (!bitmap) {
        return NULL;
    }
    
    bitmap->bit_count = size;
    bitmap->block_count = calculate_blocks(size);
    bitmap->blocks = (uint64_t *)calloc(bitmap->block_count, sizeof(uint64_t));
    if (!bitmap->blocks) {
        free(bitmap);
        return NULL;
    }
    
    return bitmap;
}

// 销毁位图
void bitmap_destroy(bitmap *bitmap) {
    if (bitmap) {
        if (bitmap->blocks) {
            free(bitmap->blocks);
        }
        free(bitmap);
    }
}

// 设置位图中的位
bool bitmap_set(bitmap *bitmap, uint32_t position) {
    if (!bitmap || position >= bitmap->bit_count) {
        return false;
    }
    
    uint32_t block_idx = position / BITMAP_INDEX_BLOCK_SIZE;
    uint32_t bit_idx = position % BITMAP_INDEX_BLOCK_SIZE;
    bitmap->blocks[block_idx] |= (1ULL << bit_idx);
    return true;
}

// 清除位图中的位
bool bitmap_clear(bitmap *bitmap, uint32_t position) {
    if (!bitmap || position >= bitmap->bit_count) {
        return false;
    }
    
    uint32_t block_idx = position / BITMAP_INDEX_BLOCK_SIZE;
    uint32_t bit_idx = position % BITMAP_INDEX_BLOCK_SIZE;
    bitmap->blocks[block_idx] &= ~(1ULL << bit_idx);
    return true;
}

// 检查位图中的位是否设置
bool bitmap_test(bitmap *bitmap, uint32_t position) {
    if (!bitmap || position >= bitmap->bit_count) {
        return false;
    }
    
    uint32_t block_idx = position / BITMAP_INDEX_BLOCK_SIZE;
    uint32_t bit_idx = position % BITMAP_INDEX_BLOCK_SIZE;
    return (bitmap->blocks[block_idx] & (1ULL << bit_idx)) != 0;
}

// 位图与操作
bitmap *bitmap_and(const bitmap *a, const bitmap *b) {
    if (!a || !b) {
        return NULL;
    }
    
    uint32_t size = a->bit_count < b->bit_count ? a->bit_count : b->bit_count;
    bitmap *result = bitmap_create(size);
    if (!result) {
        return NULL;
    }
    
    uint32_t min_blocks = result->block_count;
    for (uint32_t i = 0; i < min_blocks; i++) {
        result->blocks[i] = a->blocks[i] & b->blocks[i];
    }
    
    return result;
}

// 位图或操作
bitmap *bitmap_or(const bitmap *a, const bitmap *b) {
    if (!a || !b) {
        return NULL;
    }
    
    uint32_t size = a->bit_count > b->bit_count ? a->bit_count : b->bit_count;
    bitmap *result = bitmap_create(size);
    if (!result) {
        return NULL;
    }
    
    uint32_t min_blocks = a->block_count < b->block_count ? a->block_count : b->block_count;
    for (uint32_t i = 0; i < min_blocks; i++) {
        result->blocks[i] = a->blocks[i] | b->blocks[i];
    }
    
    // 复制剩余的块
    if (a->block_count > b->block_count) {
        for (uint32_t i = min_blocks; i < a->block_count; i++) {
            result->blocks[i] = a->blocks[i];
        }
    } else {
        for (uint32_t i = min_blocks; i < b->block_count; i++) {
            result->blocks[i] = b->blocks[i];
        }
    }
    
    return result;
}

// 位图非操作
bitmap *bitmap_not(const bitmap *bitmap) {
    if (!bitmap) {
        return NULL;
    }
    
    bitmap *result = bitmap_create(bitmap->bit_count);
    if (!result) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < bitmap->block_count; i++) {
        result->blocks[i] = ~bitmap->blocks[i];
    }
    
    // 处理最后一个块的未使用位
    if (bitmap->bit_count % BITMAP_INDEX_BLOCK_SIZE != 0) {
        uint32_t last_block_bits = bitmap->bit_count % BITMAP_INDEX_BLOCK_SIZE;
        uint64_t mask = (1ULL << last_block_bits) - 1;
        result->blocks[bitmap->block_count - 1] &= mask;
    }
    
    return result;
}

// 查找位图索引项
static int find_bitmap_index_item(bitmap_index *index, const char *value, uint32_t value_size) {
    for (uint32_t i = 0; i < index->item_count; i++) {
        if (index->items[i]->value_size == value_size && 
            memcmp(index->items[i]->value, value, value_size) == 0) {
            return i;
        }
    }
    return -1;
}

// 扩展位图大小
static bool expand_bitmaps(bitmap_index *index, uint32_t new_size) {
    for (uint32_t i = 0; i < index->item_count; i++) {
        bitmap *old_bitmap = index->items[i]->bits;
        bitmap *new_bitmap = bitmap_create(new_size);
        if (!new_bitmap) {
            return false;
        }
        
        // 复制旧位图数据
        uint32_t copy_blocks = old_bitmap->block_count;
        if (copy_blocks > new_bitmap->block_count) {
            copy_blocks = new_bitmap->block_count;
        }
        memcpy(new_bitmap->blocks, old_bitmap->blocks, copy_blocks * sizeof(uint64_t));
        
        bitmap_destroy(old_bitmap);
        index->items[i]->bits = new_bitmap;
    }
    
    return true;
}

// 初始化位图索引
bitmap_index *bitmap_index_create(uint32_t capacity) {
    if (capacity == 0) {
        capacity = 16;
    }
    
    bitmap_index *index = (bitmap_index *)malloc(sizeof(bitmap_index));
    if (!index) {
        return NULL;
    }
    
    index->items = (bitmap_index_item **)calloc(capacity, sizeof(bitmap_index_item *));
    if (!index->items) {
        free(index);
        return NULL;
    }
    
    index->item_count = 0;
    index->capacity = capacity;
    index->max_bit_position = 0;
    
    return index;
}

// 销毁位图索引
void bitmap_index_destroy(bitmap_index *index) {
    if (index) {
        for (uint32_t i = 0; i < index->item_count; i++) {
            if (index->items[i]) {
                if (index->items[i]->value) {
                    free(index->items[i]->value);
                }
                if (index->items[i]->bits) {
                    bitmap_destroy(index->items[i]->bits);
                }
                free(index->items[i]);
            }
        }
        if (index->items) {
            free(index->items);
        }
        free(index);
    }
}

// 插入数据到位图索引
bool bitmap_index_insert(bitmap_index *index, const char *value, uint32_t value_size, uint32_t row_id) {
    if (!index || !value) {
        return false;
    }
    
    // 检查是否需要扩展位图大小
    if (row_id >= index->max_bit_position) {
        uint32_t new_size = row_id + 1;
        if (!expand_bitmaps(index, new_size)) {
            return false;
        }
        index->max_bit_position = new_size;
    }
    
    // 查找或创建索引项
    int item_index = find_bitmap_index_item(index, value, value_size);
    if (item_index == -1) {
        // 创建新索引项
        if (index->item_count >= index->capacity) {
            // 扩展容量
            uint32_t new_capacity = index->capacity * 2;
            bitmap_index_item **new_items = (bitmap_index_item **)realloc(
                index->items, new_capacity * sizeof(bitmap_index_item *)
            );
            if (!new_items) {
                return false;
            }
            index->items = new_items;
            index->capacity = new_capacity;
        }
        
        // 创建新项
        bitmap_index_item *item = (bitmap_index_item *)malloc(sizeof(bitmap_index_item));
        if (!item) {
            return false;
        }
        
        item->value = (char *)malloc(value_size);
        if (!item->value) {
            free(item);
            return false;
        }
        memcpy(item->value, value, value_size);
        item->value_size = value_size;
        
        item->bits = bitmap_create(index->max_bit_position);
        if (!item->bits) {
            free(item->value);
            free(item);
            return false;
        }
        
        index->items[index->item_count] = item;
        item_index = index->item_count;
        index->item_count++;
    }
    
    // 设置位图中的位
    return bitmap_set(index->items[item_index]->bits, row_id);
}

// 查询特定值的位图
bitmap *bitmap_index_query(bitmap_index *index, const char *value, uint32_t value_size) {
    if (!index || !value) {
        return NULL;
    }
    
    int item_index = find_bitmap_index_item(index, value, value_size);
    if (item_index == -1) {
        return NULL;
    }
    
    // 创建位图副本
    bitmap *result = bitmap_create(index->items[item_index]->bits->bit_count);
    if (!result) {
        return NULL;
    }
    
    memcpy(result->blocks, index->items[item_index]->bits->blocks, 
           result->block_count * sizeof(uint64_t));
    
    return result;
}

// 获取位图索引中的所有值
char **bitmap_index_get_values(bitmap_index *index, uint32_t *value_counts, uint32_t *result_count) {
    if (!index || !result_count) {
        return NULL;
    }
    
    *result_count = index->item_count;
    char **values = (char **)malloc(index->item_count * sizeof(char *));
    if (!values) {
        return NULL;
    }
    
    if (value_counts) {
        *value_counts = (uint32_t *)malloc(index->item_count * sizeof(uint32_t));
        if (!*value_counts) {
            free(values);
            return NULL;
        }
    }
    
    for (uint32_t i = 0; i < index->item_count; i++) {
        values[i] = (char *)malloc(index->items[i]->value_size + 1);
        if (!values[i]) {
            // 清理已分配的内存
            for (uint32_t j = 0; j < i; j++) {
                free(values[j]);
            }
            free(values);
            if (value_counts) {
                free(*value_counts);
            }
            return NULL;
        }
        memcpy(values[i], index->items[i]->value, index->items[i]->value_size);
        values[i][index->items[i]->value_size] = '\0';
        
        if (value_counts) {
            // 计算设置的位数量
            uint32_t count = 0;
            bitmap *bits = index->items[i]->bits;
            for (uint32_t j = 0; j < bits->block_count; j++) {
                count += __builtin_popcountll(bits->blocks[j]);
            }
            (*value_counts)[i] = count;
        }
    }
    
    return values;
}

// 获取位图索引大小
uint32_t bitmap_index_size(bitmap_index *index) {
    if (!index) {
        return 0;
    }
    return index->item_count;
}
