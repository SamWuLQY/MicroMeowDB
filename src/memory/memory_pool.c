#include "memory_pool.h"
#include "../config/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 获取内存块大小索引
static int get_block_size_index(size_t size) {
    if (size <= MEMORY_BLOCK_SIZE_8) return 0;
    if (size <= MEMORY_BLOCK_SIZE_16) return 1;
    if (size <= MEMORY_BLOCK_SIZE_32) return 2;
    if (size <= MEMORY_BLOCK_SIZE_64) return 3;
    if (size <= MEMORY_BLOCK_SIZE_128) return 4;
    if (size <= MEMORY_BLOCK_SIZE_256) return 5;
    if (size <= MEMORY_BLOCK_SIZE_512) return 6;
    if (size <= MEMORY_BLOCK_SIZE_1024) return 7;
    if (size <= MEMORY_BLOCK_SIZE_2048) return 8;
    if (size <= MEMORY_BLOCK_SIZE_4096) return 9;
    return -1; // 超出最大块大小
}

// 初始化内存池
MemoryPool* memory_pool_init(config_system *config) {
    // 从配置系统获取内存池大小，默认512MB
    size_t initial_size = config ? config_get_int(config, "memory.memory_pool_size", 512) * 1024 * 1024 : 512 * 1024 * 1024;
    
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) {
        fprintf(stderr, "Failed to allocate memory for memory pool\n");
        return NULL;
    }

    // 初始化内存块大小数组
    pool->block_sizes[0] = MEMORY_BLOCK_SIZE_8;
    pool->block_sizes[1] = MEMORY_BLOCK_SIZE_16;
    pool->block_sizes[2] = MEMORY_BLOCK_SIZE_32;
    pool->block_sizes[3] = MEMORY_BLOCK_SIZE_64;
    pool->block_sizes[4] = MEMORY_BLOCK_SIZE_128;
    pool->block_sizes[5] = MEMORY_BLOCK_SIZE_256;
    pool->block_sizes[6] = MEMORY_BLOCK_SIZE_512;
    pool->block_sizes[7] = MEMORY_BLOCK_SIZE_1024;
    pool->block_sizes[8] = MEMORY_BLOCK_SIZE_2048;
    pool->block_sizes[9] = MEMORY_BLOCK_SIZE_4096;

    // 初始化空闲链表
    for (int i = 0; i < MEMORY_POOL_BLOCK_SIZE_COUNT; i++) {
        pool->free_lists[i] = NULL;
    }

    // 初始化内存统计
    pool->total_memory = initial_size;
    pool->used_memory = 0;
    pool->free_memory = initial_size;

    // 分配大内存区域
    pool->large_memory = (uint8_t*)malloc(initial_size);
    if (!pool->large_memory) {
        fprintf(stderr, "Failed to allocate large memory\n");
        free(pool);
        return NULL;
    }

    pool->large_memory_size = initial_size;
    pool->large_memory_used = 0;

    return pool;
}

// 分配内存
void* memory_pool_alloc(MemoryPool* pool, size_t size) {
    if (!pool || size == 0) {
        return NULL;
    }

    // 检查是否需要大内存分配
    if (size > MEMORY_POOL_MAX_BLOCK_SIZE) {
        // 检查大内存区域是否足够
        if (pool->large_memory_used + size > pool->large_memory_size) {
            // 扩展大内存区域
            size_t new_size = pool->large_memory_size * 2;
            if (new_size < pool->large_memory_used + size) {
                new_size = pool->large_memory_used + size;
            }

            uint8_t* new_large_memory = (uint8_t*)realloc(pool->large_memory, new_size);
            if (!new_large_memory) {
                fprintf(stderr, "Failed to expand large memory\n");
                return NULL;
            }

            pool->large_memory = new_large_memory;
            pool->large_memory_size = new_size;
            pool->total_memory = new_size;
            pool->free_memory = new_size - pool->used_memory;
        }

        // 分配大内存
        void* ptr = &pool->large_memory[pool->large_memory_used];
        pool->large_memory_used += size;
        pool->used_memory += size;
        pool->free_memory -= size;

        return ptr;
    }

    // 获取内存块大小索引
    int index = get_block_size_index(size);
    if (index < 0) {
        return NULL;
    }

    size_t block_size = pool->block_sizes[index];

    // 检查空闲链表是否有可用块
    if (pool->free_lists[index]) {
        // 从空闲链表中获取一个块
        MemoryBlock* block = pool->free_lists[index];
        pool->free_lists[index] = block->next;
        if (block->next) {
            block->next->prev = NULL;
        }

        block->in_use = true;
        block->next = NULL;
        block->prev = NULL;

        pool->used_memory += block_size;
        pool->free_memory -= block_size;

        return block->data;
    }

    // 没有可用块，从大内存区域分配
    if (pool->large_memory_used + block_size > pool->large_memory_size) {
        // 扩展大内存区域
        size_t new_size = pool->large_memory_size * 2;
        if (new_size < pool->large_memory_used + block_size) {
            new_size = pool->large_memory_used + block_size;
        }

        uint8_t* new_large_memory = (uint8_t*)realloc(pool->large_memory, new_size);
        if (!new_large_memory) {
            fprintf(stderr, "Failed to expand large memory\n");
            return NULL;
        }

        pool->large_memory = new_large_memory;
        pool->large_memory_size = new_size;
        pool->total_memory = new_size;
        pool->free_memory = new_size - pool->used_memory;
    }

    // 创建新的内存块
    MemoryBlock* block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    if (!block) {
        fprintf(stderr, "Failed to allocate memory block\n");
        return NULL;
    }

    block->data = &pool->large_memory[pool->large_memory_used];
    block->size = block_size;
    block->in_use = true;
    block->next = NULL;
    block->prev = NULL;

    pool->large_memory_used += block_size;
    pool->used_memory += block_size;
    pool->free_memory -= block_size;

    return block->data;
}

// 释放内存
void memory_pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }

    // 检查是否是大内存分配
    if (ptr >= pool->large_memory && ptr < &pool->large_memory[pool->large_memory_size]) {
        // 大内存释放（简单处理，实际需要更复杂的管理）
        // 这里暂时不处理，因为大内存分配是连续的
        // 实际实现中需要使用内存块管理大内存
        fprintf(stderr, "Large memory free not implemented\n");
        return;
    }

    // 查找内存块
    // 这里简化处理，实际需要维护内存块的映射
    // 实际实现中应该使用哈希表或其他数据结构来快速查找内存块
    fprintf(stderr, "Memory block lookup not implemented\n");
}

// 获取内存池状态
void memory_pool_status(MemoryPool* pool) {
    if (!pool) {
        return;
    }

    printf("Memory Pool Status:\n");
    printf("Total Memory: %zu bytes\n", pool->total_memory);
    printf("Used Memory: %zu bytes\n", pool->used_memory);
    printf("Free Memory: %zu bytes\n", pool->free_memory);
    printf("Large Memory Size: %zu bytes\n", pool->large_memory_size);
    printf("Large Memory Used: %zu bytes\n", pool->large_memory_used);

    // 打印各大小内存块的使用情况
    for (int i = 0; i < MEMORY_POOL_BLOCK_SIZE_COUNT; i++) {
        size_t block_size = pool->block_sizes[i];
        int free_count = 0;
        MemoryBlock* current = pool->free_lists[i];
        while (current) {
            free_count++;
            current = current->next;
        }
        printf("Block Size %zu: %d free blocks\n", block_size, free_count);
    }
}

// 内存碎片整理
void memory_pool_defragment(MemoryPool* pool) {
    if (!pool) {
        return;
    }

    // 简单的内存碎片整理实现
    // 实际实现中需要更复杂的算法
    printf("Memory defragmentation not implemented\n");
}

// 销毁内存池
void memory_pool_destroy(MemoryPool* pool) {
    if (!pool) {
        return;
    }

    // 释放所有内存块
    for (int i = 0; i < MEMORY_POOL_BLOCK_SIZE_COUNT; i++) {
        MemoryBlock* current = pool->free_lists[i];
        while (current) {
            MemoryBlock* next = current->next;
            free(current);
            current = next;
        }
    }

    // 释放大内存区域
    if (pool->large_memory) {
        free(pool->large_memory);
    }

    // 释放内存池结构
    free(pool);
}