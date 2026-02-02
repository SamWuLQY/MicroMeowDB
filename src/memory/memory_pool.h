#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdint.h>
#include <stdbool.h>

// 前向声明
struct config_system;

// 内存块大小定义
#define MEMORY_BLOCK_SIZE_8 8
#define MEMORY_BLOCK_SIZE_16 16
#define MEMORY_BLOCK_SIZE_32 32
#define MEMORY_BLOCK_SIZE_64 64
#define MEMORY_BLOCK_SIZE_128 128
#define MEMORY_BLOCK_SIZE_256 256
#define MEMORY_BLOCK_SIZE_512 512
#define MEMORY_BLOCK_SIZE_1024 1024
#define MEMORY_BLOCK_SIZE_2048 2048
#define MEMORY_BLOCK_SIZE_4096 4096

// 内存池配置
#define MEMORY_POOL_PAGE_SIZE 4096
#define MEMORY_POOL_MAX_BLOCK_SIZE 4096
#define MEMORY_POOL_BLOCK_SIZE_COUNT 10

// 内存块结构
typedef struct {
    uint8_t* data;
    size_t size;
    bool in_use;
    struct MemoryBlock* next;
    struct MemoryBlock* prev;
} MemoryBlock;

// 内存池结构
typedef struct {
    MemoryBlock* free_lists[MEMORY_POOL_BLOCK_SIZE_COUNT];
    size_t block_sizes[MEMORY_POOL_BLOCK_SIZE_COUNT];
    size_t total_memory;
    size_t used_memory;
    size_t free_memory;
    uint8_t* large_memory; // 用于大内存分配
    size_t large_memory_size;
    size_t large_memory_used;
} MemoryPool;

// 初始化内存池
MemoryPool* memory_pool_init(struct config_system *config);

// 分配内存
void* memory_pool_alloc(MemoryPool* pool, size_t size);

// 释放内存
void memory_pool_free(MemoryPool* pool, void* ptr);

// 获取内存池状态
void memory_pool_status(MemoryPool* pool);

// 销毁内存池
void memory_pool_destroy(MemoryPool* pool);

// 内存碎片整理
void memory_pool_defragment(MemoryPool* pool);

#endif // MEMORY_POOL_H