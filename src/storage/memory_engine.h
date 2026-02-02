#ifndef MEMORY_ENGINE_H
#define MEMORY_ENGINE_H

#include "storage_engine.h"

// 内存表引擎行数据结构
typedef struct {
    Row* row;
    uint64_t row_id;
    struct MemoryEngineRow* next;
    struct MemoryEngineRow* prev;
} MemoryEngineRow;

// 内存表引擎表数据结构
typedef struct {
    Table* table;
    MemoryEngineRow** rows; // 哈希表
    size_t row_count;
    size_t capacity;
    uint64_t next_row_id;
    uint64_t transaction_id;
    bool in_transaction;
    bool persistent; // 是否持久化
    char* persist_file; // 持久化文件路径
} MemoryEngineTableData;

// 内存表引擎数据结构
typedef struct {
    MemoryEngineTableData** tables;
    size_t table_count;
    uint64_t next_transaction_id;
} MemoryEngineData;

// 创建内存表引擎
StorageEngine* create_memory_engine(void* config);

// 内存表引擎表操作
bool memory_engine_create_table(StorageEngine* engine, Table* table);
bool memory_engine_drop_table(StorageEngine* engine, const char* table_name);
Table* memory_engine_get_table(StorageEngine* engine, const char* table_name);

// 内存表引擎数据操作
bool memory_engine_insert(StorageEngine* engine, const char* table_name, Row* row);
bool memory_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row);
bool memory_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id);
Row* memory_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id);
bool memory_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count);

// 内存表引擎事务操作
bool memory_engine_begin_transaction(StorageEngine* engine);
bool memory_engine_commit_transaction(StorageEngine* engine);
bool memory_engine_rollback_transaction(StorageEngine* engine);

// 内存表引擎特定操作
bool memory_engine_optimize(StorageEngine* engine, const char* table_name);
bool memory_engine_checkpoint(StorageEngine* engine);

// 内存表引擎销毁
void memory_engine_destroy(StorageEngine* engine);

// 内存表引擎辅助函数
MemoryEngineTableData* memory_engine_get_table_data(StorageEngine* engine, const char* table_name);
bool memory_engine_expand_table(StorageEngine* engine, MemoryEngineTableData* table_data, size_t new_capacity);
uint64_t memory_engine_hash(uint64_t row_id, size_t capacity);
bool memory_engine_persist_table(MemoryEngineTableData* table_data);
bool memory_engine_load_table(MemoryEngineTableData* table_data);

#endif // MEMORY_ENGINE_H