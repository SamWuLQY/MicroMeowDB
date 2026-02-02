#ifndef COLUMN_ENGINE_H
#define COLUMN_ENGINE_H

#include "storage_engine.h"

// 列存引擎列数据结构
typedef struct {
    Column* column;
    void** values;
    size_t value_count;
    size_t capacity;
    bool* null_mask;
} ColumnEngineColumnData;

// 列存引擎表数据结构
typedef struct {
    Table* table;
    ColumnEngineColumnData** columns;
    size_t column_count;
    size_t row_count;
    size_t capacity;
    uint64_t next_row_id;
    uint64_t transaction_id;
    bool in_transaction;
} ColumnEngineTableData;

// 列存引擎数据结构
typedef struct {
    ColumnEngineTableData** tables;
    size_t table_count;
    uint64_t next_transaction_id;
} ColumnEngineData;

// 创建列存引擎
StorageEngine* create_column_engine(void* config);

// 列存引擎表操作
bool column_engine_create_table(StorageEngine* engine, Table* table);
bool column_engine_drop_table(StorageEngine* engine, const char* table_name);
Table* column_engine_get_table(StorageEngine* engine, const char* table_name);

// 列存引擎数据操作
bool column_engine_insert(StorageEngine* engine, const char* table_name, Row* row);
bool column_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row);
bool column_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id);
Row* column_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id);
bool column_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count);

// 列存引擎事务操作
bool column_engine_begin_transaction(StorageEngine* engine);
bool column_engine_commit_transaction(StorageEngine* engine);
bool column_engine_rollback_transaction(StorageEngine* engine);

// 列存引擎特定操作
bool column_engine_optimize(StorageEngine* engine, const char* table_name);
bool column_engine_checkpoint(StorageEngine* engine);

// 列存引擎销毁
void column_engine_destroy(StorageEngine* engine);

// 列存引擎辅助函数
ColumnEngineTableData* column_engine_get_table_data(StorageEngine* engine, const char* table_name);
bool column_engine_expand_table(StorageEngine* engine, ColumnEngineTableData* table_data, size_t new_capacity);
bool column_engine_expand_column(ColumnEngineColumnData* column_data, size_t new_capacity);

#endif // COLUMN_ENGINE_H