#include "bloom_filter.h"
#include <stdlib.h>
#include <string.h>

// 哈希函数1：FNV-1a
static uint32_t hash_function1(const char *key, uint32_t key_size) {
    uint32_t hash = 2166136261U;
    for (uint32_t i = 0; i < key_size; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619U;
    }
    return hash;
}

// 哈希函数2：MurmurHash2
static uint32_t hash_function2(const char *key, uint32_t key_size) {
    uint32_t hash = 0xdeadbeef;
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;
    uint32_t r1 = 15;
    uint32_t r2 = 13;
    uint32_t m = 5;
    uint32_t n = 0xe6546b64;
    
    const uint8_t *data = (const uint8_t *)key;
    uint32_t len = key_size;
    
    while (len >= 4) {
        uint32_t k = *(uint32_t *)data;
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;
        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
        data += 4;
        len -= 4;
    }
    
    uint32_t k = 0;
    switch (len) {
        case 3:
            k ^= data[2] << 16;
        case 2:
            k ^= data[1] << 8;
        case 1:
            k ^= data[0];
            k *= c1;
            k = (k << r1) | (k >> (32 - r1));
            k *= c2;
            hash ^= k;
    }
    
    hash ^= key_size;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);
    
    return hash;
}

// 哈希函数3：Jenkins hash
static uint32_t hash_function3(const char *key, uint32_t key_size) {
    uint32_t hash = 0;
    for (uint32_t i = 0; i < key_size; i++) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

// 获取多个哈希值
static void get_hash_values(const char *key, uint32_t key_size, uint32_t size, uint32_t hash_count, uint32_t *hashes) {
    hashes[0] = hash_function1(key, key_size) % size;
    if (hash_count > 1) {
        hashes[1] = hash_function2(key, key_size) % size;
    }
    if (hash_count > 2) {
        hashes[2] = hash_function3(key, key_size) % size;
    }
    // 对于更多哈希函数，使用双重哈希技术
    for (uint32_t i = 3; i < hash_count; i++) {
        hashes[i] = (hashes[0] + i * hashes[1]) % size;
    }
}

// 初始化布隆过滤器
bloom_filter *bloom_filter_create(uint32_t size, uint32_t hash_count) {
    if (size == 0) {
        size = BLOOM_FILTER_DEFAULT_SIZE;
    }
    if (hash_count == 0) {
        hash_count = BLOOM_FILTER_DEFAULT_HASHES;
    }
    
    bloom_filter *filter = (bloom_filter *)malloc(sizeof(bloom_filter));
    if (!filter) {
        return NULL;
    }
    
    filter->bits = (uint8_t *)calloc((size + 7) / 8, 1);
    if (!filter->bits) {
        free(filter);
        return NULL;
    }
    
    filter->size = size;
    filter->hash_count = hash_count;
    filter->item_count = 0;
    
    return filter;
}

// 销毁布隆过滤器
void bloom_filter_destroy(bloom_filter *filter) {
    if (filter) {
        if (filter->bits) {
            free(filter->bits);
        }
        free(filter);
    }
}

// 添加元素到布隆过滤器
bool bloom_filter_add(bloom_filter *filter, const char *key, uint32_t key_size) {
    if (!filter || !key) {
        return false;
    }
    
    uint32_t hashes[16]; // 最多支持16个哈希函数
    get_hash_values(key, key_size, filter->size, filter->hash_count, hashes);
    
    // 设置对应的位
    for (uint32_t i = 0; i < filter->hash_count; i++) {
        uint32_t bit_pos = hashes[i];
        uint32_t byte_pos = bit_pos / 8;
        uint32_t bit_offset = bit_pos % 8;
        filter->bits[byte_pos] |= (1 << bit_offset);
    }
    
    filter->item_count++;
    return true;
}

// 检查元素是否可能在布隆过滤器中
bool bloom_filter_contains(bloom_filter *filter, const char *key, uint32_t key_size) {
    if (!filter || !key) {
        return false;
    }
    
    uint32_t hashes[16]; // 最多支持16个哈希函数
    get_hash_values(key, key_size, filter->size, filter->hash_count, hashes);
    
    // 检查对应的位
    for (uint32_t i = 0; i < filter->hash_count; i++) {
        uint32_t bit_pos = hashes[i];
        uint32_t byte_pos = bit_pos / 8;
        uint32_t bit_offset = bit_pos % 8;
        if (!(filter->bits[byte_pos] & (1 << bit_offset))) {
            return false; // 肯定不存在
        }
    }
    
    return true; // 可能存在
}

// 重置布隆过滤器
bool bloom_filter_reset(bloom_filter *filter) {
    if (!filter) {
        return false;
    }
    
    memset(filter->bits, 0, (filter->size + 7) / 8);
    filter->item_count = 0;
    return true;
}

// 获取布隆过滤器大小
uint32_t bloom_filter_size(bloom_filter *filter) {
    if (!filter) {
        return 0;
    }
    return filter->size;
}

// 获取布隆过滤器中元素数量
uint32_t bloom_filter_item_count(bloom_filter *filter) {
    if (!filter) {
        return 0;
    }
    return filter->item_count;
}
