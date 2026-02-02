#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 前向声明
struct config_system;

// 存储引擎类型定义
#define STORAGE_ENGINE_ROW 0 // 行存引擎
#define STORAGE_ENGINE_COLUMN 1 // 列存引擎
#define STORAGE_ENGINE_MEMORY 2 // 内存表引擎

// 数据类型定义
#define DATA_TYPE_INT 0
#define DATA_TYPE_BIGINT 1
#define DATA_TYPE_FLOAT 2
#define DATA_TYPE_DOUBLE 3
#define DATA_TYPE_CHAR 4
#define DATA_TYPE_VARCHAR 5
#define DATA_TYPE_DATE 6
#define DATA_TYPE_DATETIME 7
#define DATA_TYPE_BOOLEAN 8
#define DATA_TYPE_BLOB 9

// 列结构
typedef struct {
    char* name;
    int data_type;
    size_t length;
    bool nullable;
    bool primary_key;
    bool auto_increment;
    void* default_value;
} Column;

// 表结构
typedef struct {
    char* name;
    Column* columns;
    size_t column_count;
    size_t row_count;
    int storage_engine_type;
    void* engine_specific_data;
} Table;

// 行数据结构
typedef struct {
    void** values;
    size_t value_count;
    bool deleted;
    uint64_t version;
} Row;

// 存储引擎接口
typedef struct {
    int type;
    char* name;
    
    // 表操作
    bool (*create_table)(struct StorageEngine* engine, Table* table);
    bool (*drop_table)(struct StorageEngine* engine, const char* table_name);
    Table* (*get_table)(struct StorageEngine* engine, const char* table_name);
    
    // 数据操作
    bool (*insert)(struct StorageEngine* engine, const char* table_name, Row* row);
    bool (*update)(struct StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row);
    bool (*delete)(struct StorageEngine* engine, const char* table_name, uint64_t row_id);
    Row* (*select)(struct StorageEngine* engine, const char* table_name, uint64_t row_id);
    
    // 批量操作
    bool (*batch_insert)(struct StorageEngine* engine, const char* table_name, Row** rows, size_t row_count);
    
    // 事务操作
    bool (*begin_transaction)(struct StorageEngine* engine);
    bool (*commit_transaction)(struct StorageEngine* engine);
    bool (*rollback_transaction)(struct StorageEngine* engine);
    
    // 引擎特定操作
    bool (*optimize)(struct StorageEngine* engine, const char* table_name);
    bool (*checkpoint)(struct StorageEngine* engine);
    
    // 销毁引擎
    void (*destroy)(struct StorageEngine* engine);
    
    // 引擎特定数据
    void* data;
} StorageEngine;

// 存储引擎管理器结构
typedef struct {
    StorageEngine* engines[3]; // 三种存储引擎
    Table** tables;
    size_t table_count;
} StorageEngineManager;

// 初始化存储引擎管理器
StorageEngineManager* storage_engine_manager_init(struct config_system *config);

// 创建存储引擎
StorageEngine* storage_engine_create(int type, struct config_system *config);

// 注册存储引擎
bool storage_engine_register(StorageEngineManager* manager, StorageEngine* engine);

// 创建表
bool storage_engine_create_table(StorageEngineManager* manager, Table* table);

// 删除表
bool storage_engine_drop_table(StorageEngineManager* manager, const char* table_name);

// 获取表
Table* storage_engine_get_table(StorageEngineManager* manager, const char* table_name);

// 插入数据
bool storage_engine_insert(StorageEngineManager* manager, const char* table_name, Row* row);

// 更新数据
bool storage_engine_update(StorageEngineManager* manager, const char* table_name, uint64_t row_id, Row* row);

// 删除数据
bool storage_engine_delete(StorageEngineManager* manager, const char* table_name, uint64_t row_id);

// 查询数据
Row* storage_engine_select(StorageEngineManager* manager, const char* table_name, uint64_t row_id);

// 批量插入数据
bool storage_engine_batch_insert(StorageEngineManager* manager, const char* table_name, Row** rows, size_t row_count);

// 开始事务
bool storage_engine_begin_transaction(StorageEngineManager* manager, const char* table_name);

// 提交事务
bool storage_engine_commit_transaction(StorageEngineManager* manager, const char* table_name);

// 回滚事务
bool storage_engine_rollback_transaction(StorageEngineManager* manager, const char* table_name);

// 优化表
bool storage_engine_optimize(StorageEngineManager* manager, const char* table_name);

// 执行检查点
bool storage_engine_checkpoint(StorageEngineManager* manager, int engine_type);

// 销毁存储引擎管理器
void storage_engine_manager_destroy(StorageEngineManager* manager);

// 辅助函数
Column* create_column(const char* name, int data_type, size_t length, bool nullable, bool primary_key, bool auto_increment, void* default_value);
Table* create_table(const char* name, Column* columns, size_t column_count, int storage_engine_type);
Row* create_row(size_t column_count);
void destroy_row(Row* row);
void destroy_table(Table* table);
void destroy_column(Column* column);

#endif // STORAGE_ENGINE_H