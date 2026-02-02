#include "storage_engine.h"
#include "../config/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 存储引擎实现前向声明
static StorageEngine* create_row_storage_engine(config_system *config);
static StorageEngine* create_column_storage_engine(config_system *config);
static StorageEngine* create_memory_storage_engine(config_system *config);

// 初始化存储引擎管理器
StorageEngineManager* storage_engine_manager_init(config_system *config) {
    StorageEngineManager* manager = (StorageEngineManager*)malloc(sizeof(StorageEngineManager));
    if (!manager) {
        return NULL;
    }

    manager->tables = NULL;
    manager->table_count = 0;

    // 初始化三种存储引擎
    manager->engines[STORAGE_ENGINE_ROW] = create_row_storage_engine(config);
    manager->engines[STORAGE_ENGINE_COLUMN] = create_column_storage_engine(config);
    manager->engines[STORAGE_ENGINE_MEMORY] = create_memory_storage_engine(config);

    return manager;
}

// 创建存储引擎
StorageEngine* storage_engine_create(int type, config_system *config) {
    switch (type) {
        case STORAGE_ENGINE_ROW:
            return create_row_storage_engine(config);
        case STORAGE_ENGINE_COLUMN:
            return create_column_storage_engine(config);
        case STORAGE_ENGINE_MEMORY:
            return create_memory_storage_engine(config);
        default:
            fprintf(stderr, "Invalid storage engine type\n");
            return NULL;
    }
}

// 注册存储引擎
bool storage_engine_register(StorageEngineManager* manager, StorageEngine* engine) {
    if (!manager || !engine) {
        return false;
    }

    if (engine->type < 0 || engine->type >= 3) {
        fprintf(stderr, "Invalid storage engine type\n");
        return false;
    }

    manager->engines[engine->type] = engine;
    return true;
}

// 创建表
bool storage_engine_create_table(StorageEngineManager* manager, Table* table) {
    if (!manager || !table) {
        return false;
    }

    // 检查表是否已存在
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table->name) == 0) {
            fprintf(stderr, "Table already exists\n");
            return false;
        }
    }

    // 选择存储引擎
    int engine_type = table->storage_engine_type;
    if (engine_type < 0 || engine_type >= 3 || !manager->engines[engine_type]) {
        fprintf(stderr, "Invalid storage engine type\n");
        return false;
    }

    // 调用存储引擎的创建表方法
    if (!manager->engines[engine_type]->create_table(manager->engines[engine_type], table)) {
        return false;
    }

    // 添加到表列表
    Table** new_tables = (Table**)realloc(manager->tables, sizeof(Table*) * (manager->table_count + 1));
    if (!new_tables) {
        return false;
    }

    new_tables[manager->table_count] = table;
    manager->tables = new_tables;
    manager->table_count++;

    return true;
}

// 删除表
bool storage_engine_drop_table(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    size_t table_index = (size_t)-1;
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table_index = i;
            table = manager->tables[i];
            break;
        }
    }

    if (table_index == (size_t)-1) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的删除表方法
    int engine_type = table->storage_engine_type;
    if (!manager->engines[engine_type]->drop_table(manager->engines[engine_type], table_name)) {
        return false;
    }

    // 从表列表中移除
    for (size_t i = table_index; i < manager->table_count - 1; i++) {
        manager->tables[i] = manager->tables[i + 1];
    }

    Table** new_tables = (Table**)realloc(manager->tables, sizeof(Table*) * (manager->table_count - 1));
    if (new_tables || manager->table_count == 1) {
        manager->tables = new_tables;
    }

    manager->table_count--;

    // 释放表资源
    destroy_table(table);

    return true;
}

// 获取表
Table* storage_engine_get_table(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return NULL;
    }

    // 查找表
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            // 调用存储引擎的获取表方法
            int engine_type = manager->tables[i]->storage_engine_type;
            return manager->engines[engine_type]->get_table(manager->engines[engine_type], table_name);
        }
    }

    return NULL;
}

// 插入数据
bool storage_engine_insert(StorageEngineManager* manager, const char* table_name, Row* row) {
    if (!manager || !table_name || !row) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的插入方法
    int engine_type = table->storage_engine_type;
    if (!manager->engines[engine_type]->insert(manager->engines[engine_type], table_name, row)) {
        return false;
    }

    // 更新表的行数
    table->row_count++;

    return true;
}

// 更新数据
bool storage_engine_update(StorageEngineManager* manager, const char* table_name, uint64_t row_id, Row* row) {
    if (!manager || !table_name || !row) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的更新方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->update(manager->engines[engine_type], table_name, row_id, row);
}

// 删除数据
bool storage_engine_delete(StorageEngineManager* manager, const char* table_name, uint64_t row_id) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的删除方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->delete(manager->engines[engine_type], table_name, row_id);
}

// 查询数据
Row* storage_engine_select(StorageEngineManager* manager, const char* table_name, uint64_t row_id) {
    if (!manager || !table_name) {
        return NULL;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return NULL;
    }

    // 调用存储引擎的查询方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->select(manager->engines[engine_type], table_name, row_id);
}

// 批量插入数据
bool storage_engine_batch_insert(StorageEngineManager* manager, const char* table_name, Row** rows, size_t row_count) {
    if (!manager || !table_name || !rows || row_count == 0) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的批量插入方法
    int engine_type = table->storage_engine_type;
    if (!manager->engines[engine_type]->batch_insert(manager->engines[engine_type], table_name, rows, row_count)) {
        return false;
    }

    // 更新表的行数
    table->row_count += row_count;

    return true;
}

// 开始事务
bool storage_engine_begin_transaction(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的开始事务方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->begin_transaction(manager->engines[engine_type]);
}

// 提交事务
bool storage_engine_commit_transaction(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的提交事务方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->commit_transaction(manager->engines[engine_type]);
}

// 回滚事务
bool storage_engine_rollback_transaction(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的回滚事务方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->rollback_transaction(manager->engines[engine_type]);
}

// 优化表
bool storage_engine_optimize(StorageEngineManager* manager, const char* table_name) {
    if (!manager || !table_name) {
        return false;
    }

    // 查找表
    Table* table = NULL;
    for (size_t i = 0; i < manager->table_count; i++) {
        if (strcmp(manager->tables[i]->name, table_name) == 0) {
            table = manager->tables[i];
            break;
        }
    }

    if (!table) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 调用存储引擎的优化方法
    int engine_type = table->storage_engine_type;
    return manager->engines[engine_type]->optimize(manager->engines[engine_type], table_name);
}

// 执行检查点
bool storage_engine_checkpoint(StorageEngineManager* manager, int engine_type) {
    if (!manager || engine_type < 0 || engine_type >= 3) {
        return false;
    }

    if (!manager->engines[engine_type]) {
        fprintf(stderr, "Storage engine not initialized\n");
        return false;
    }

    return manager->engines[engine_type]->checkpoint(manager->engines[engine_type]);
}

// 销毁存储引擎管理器
void storage_engine_manager_destroy(StorageEngineManager* manager) {
    if (!manager) {
        return;
    }

    // 销毁所有表
    for (size_t i = 0; i < manager->table_count; i++) {
        destroy_table(manager->tables[i]);
    }
    if (manager->tables) {
        free(manager->tables);
    }

    // 销毁所有存储引擎
    for (int i = 0; i < 3; i++) {
        if (manager->engines[i]) {
            manager->engines[i]->destroy(manager->engines[i]);
        }
    }

    free(manager);
}

// 创建列
Column* create_column(const char* name, int data_type, size_t length, bool nullable, bool primary_key, bool auto_increment, void* default_value) {
    Column* column = (Column*)malloc(sizeof(Column));
    if (!column) {
        return NULL;
    }

    column->name = strdup(name);
    column->data_type = data_type;
    column->length = length;
    column->nullable = nullable;
    column->primary_key = primary_key;
    column->auto_increment = auto_increment;
    column->default_value = default_value;

    return column;
}

// 创建表
Table* create_table(const char* name, Column* columns, size_t column_count, int storage_engine_type) {
    Table* table = (Table*)malloc(sizeof(Table));
    if (!table) {
        return NULL;
    }

    table->name = strdup(name);
    table->columns = columns;
    table->column_count = column_count;
    table->row_count = 0;
    table->storage_engine_type = storage_engine_type;
    table->engine_specific_data = NULL;

    return table;
}

// 创建行
Row* create_row(size_t column_count) {
    Row* row = (Row*)malloc(sizeof(Row));
    if (!row) {
        return NULL;
    }

    row->values = (void**)malloc(sizeof(void*) * column_count);
    if (!row->values) {
        free(row);
        return NULL;
    }

    for (size_t i = 0; i < column_count; i++) {
        row->values[i] = NULL;
    }

    row->value_count = column_count;
    row->deleted = false;
    row->version = 0;

    return row;
}

// 销毁行
void destroy_row(Row* row) {
    if (!row) {
        return;
    }

    if (row->values) {
        // 释放值（假设值是动态分配的）
        for (size_t i = 0; i < row->value_count; i++) {
            if (row->values[i]) {
                free(row->values[i]);
            }
        }
        free(row->values);
    }

    free(row);
}

// 销毁表
void destroy_table(Table* table) {
    if (!table) {
        return;
    }

    if (table->name) {
        free(table->name);
    }

    if (table->columns) {
        for (size_t i = 0; i < table->column_count; i++) {
            destroy_column(&table->columns[i]);
        }
        free(table->columns);
    }

    if (table->engine_specific_data) {
        free(table->engine_specific_data);
    }

    free(table);
}

// 销毁列
void destroy_column(Column* column) {
    if (!column) {
        return;
    }

    if (column->name) {
        free(column->name);
    }

    if (column->default_value) {
        free(column->default_value);
    }
}

// 行存引擎实现
#include "row_engine.h"

// 列存引擎实现
#include "column_engine.h"

// 内存表引擎实现
#include "memory_engine.h"

// 创建行存引擎
static StorageEngine* create_row_storage_engine(void* config) {
    return create_row_engine(config);
}

// 创建列存引擎
static StorageEngine* create_column_storage_engine(void* config) {
    return create_column_engine(config);
}

// 创建内存表引擎
static StorageEngine* create_memory_storage_engine(void* config) {
    return create_memory_engine(config);
}