#ifndef ROW_ENGINE_H
#define ROW_ENGINE_H

#include "storage_engine.h"

// 行存引擎表数据结构
typedef struct {
    Table* table;
    Row** rows;
    size_t row_count;
    size_t capacity;
    uint64_t next_row_id;
    uint64_t transaction_id;
    bool in_transaction;
} RowEngineTableData;

// 行存引擎数据结构
typedef struct {
    RowEngineTableData** tables;
    size_t table_count;
    uint64_t next_transaction_id;
} RowEngineData;

// 创建行存引擎
StorageEngine* create_row_engine(void* config);

// 行存引擎表操作
bool row_engine_create_table(StorageEngine* engine, Table* table);
bool row_engine_drop_table(StorageEngine* engine, const char* table_name);
Table* row_engine_get_table(StorageEngine* engine, const char* table_name);

// 行存引擎数据操作
bool row_engine_insert(StorageEngine* engine, const char* table_name, Row* row);
bool row_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row);
bool row_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id);
Row* row_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id);
bool row_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count);

// 行存引擎事务操作
bool row_engine_begin_transaction(StorageEngine* engine);
bool row_engine_commit_transaction(StorageEngine* engine);
bool row_engine_rollback_transaction(StorageEngine* engine);

// 行存引擎特定操作
bool row_engine_optimize(StorageEngine* engine, const char* table_name);
bool row_engine_checkpoint(StorageEngine* engine);

// 行存引擎销毁
void row_engine_destroy(StorageEngine* engine);

// 行存引擎辅助函数
RowEngineTableData* row_engine_get_table_data(StorageEngine* engine, const char* table_name);
bool row_engine_expand_table(StorageEngine* engine, RowEngineTableData* table_data, size_t new_capacity);

#endif // ROW_ENGINE_H