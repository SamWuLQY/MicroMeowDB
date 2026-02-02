#include "row_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 创建行存引擎
StorageEngine* create_row_engine(void* config) {
    StorageEngine* engine = (StorageEngine*)malloc(sizeof(StorageEngine));
    if (!engine) {
        return NULL;
    }

    RowEngineData* data = (RowEngineData*)malloc(sizeof(RowEngineData));
    if (!data) {
        free(engine);
        return NULL;
    }

    data->tables = NULL;
    data->table_count = 0;
    data->next_transaction_id = 1;

    engine->type = STORAGE_ENGINE_ROW;
    engine->name = "row_engine";
    engine->data = data;

    // 设置函数指针
    engine->create_table = row_engine_create_table;
    engine->drop_table = row_engine_drop_table;
    engine->get_table = row_engine_get_table;
    engine->insert = row_engine_insert;
    engine->update = row_engine_update;
    engine->delete = row_engine_delete;
    engine->select = row_engine_select;
    engine->batch_insert = row_engine_batch_insert;
    engine->begin_transaction = row_engine_begin_transaction;
    engine->commit_transaction = row_engine_commit_transaction;
    engine->rollback_transaction = row_engine_rollback_transaction;
    engine->optimize = row_engine_optimize;
    engine->checkpoint = row_engine_checkpoint;
    engine->destroy = row_engine_destroy;

    return engine;
}

// 获取行存引擎表数据
RowEngineTableData* row_engine_get_table_data(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return NULL;
    }

    RowEngineData* data = (RowEngineData*)engine->data;
    for (size_t i = 0; i < data->table_count; i++) {
        if (strcmp(data->tables[i]->table->name, table_name) == 0) {
            return data->tables[i];
        }
    }

    return NULL;
}

// 扩展表容量
bool row_engine_expand_table(StorageEngine* engine, RowEngineTableData* table_data, size_t new_capacity) {
    if (!engine || !table_data || new_capacity <= table_data->capacity) {
        return false;
    }

    Row** new_rows = (Row**)realloc(table_data->rows, sizeof(Row*) * new_capacity);
    if (!new_rows) {
        return false;
    }

    // 初始化新分配的空间
    for (size_t i = table_data->capacity; i < new_capacity; i++) {
        new_rows[i] = NULL;
    }

    table_data->rows = new_rows;
    table_data->capacity = new_capacity;

    return true;
}

// 创建表
bool row_engine_create_table(StorageEngine* engine, Table* table) {
    if (!engine || !table) {
        return false;
    }

    RowEngineData* data = (RowEngineData*)engine->data;

    // 检查表是否已存在
    if (row_engine_get_table_data(engine, table->name)) {
        fprintf(stderr, "Table already exists\n");
        return false;
    }

    // 创建表数据结构
    RowEngineTableData* table_data = (RowEngineTableData*)malloc(sizeof(RowEngineTableData));
    if (!table_data) {
        return false;
    }

    // 初始化表数据
    table_data->table = table;
    table_data->row_count = 0;
    table_data->capacity = 1024; // 默认容量
    table_data->next_row_id = 1;
    table_data->transaction_id = 0;
    table_data->in_transaction = false;

    // 分配行数组
    table_data->rows = (Row**)malloc(sizeof(Row*) * table_data->capacity);
    if (!table_data->rows) {
        free(table_data);
        return false;
    }

    // 初始化行数组
    for (size_t i = 0; i < table_data->capacity; i++) {
        table_data->rows[i] = NULL;
    }

    // 将表数据添加到引擎
    RowEngineTableData** new_tables = (RowEngineTableData**)realloc(data->tables, sizeof(RowEngineTableData*) * (data->table_count + 1));
    if (!new_tables) {
        free(table_data->rows);
        free(table_data);
        return false;
    }

    new_tables[data->table_count] = table_data;
    data->tables = new_tables;
    data->table_count++;

    // 设置表的引擎特定数据
    table->engine_specific_data = table_data;

    return true;
}

// 删除表
bool row_engine_drop_table(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    RowEngineData* data = (RowEngineData*)engine->data;

    // 查找表
    size_t table_index = (size_t)-1;
    RowEngineTableData* table_data = NULL;
    for (size_t i = 0; i < data->table_count; i++) {
        if (strcmp(data->tables[i]->table->name, table_name) == 0) {
            table_index = i;
            table_data = data->tables[i];
            break;
        }
    }

    if (table_index == (size_t)-1) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 释放表数据
    if (table_data->rows) {
        for (size_t i = 0; i < table_data->row_count; i++) {
            if (table_data->rows[i]) {
                destroy_row(table_data->rows[i]);
            }
        }
        free(table_data->rows);
    }
    free(table_data);

    // 从引擎中移除表
    for (size_t i = table_index; i < data->table_count - 1; i++) {
        data->tables[i] = data->tables[i + 1];
    }

    RowEngineTableData** new_tables = (RowEngineTableData**)realloc(data->tables, sizeof(RowEngineTableData*) * (data->table_count - 1));
    if (new_tables || data->table_count == 1) {
        data->tables = new_tables;
    }

    data->table_count--;

    return true;
}

// 获取表
Table* row_engine_get_table(StorageEngine* engine, const char* table_name) {
    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        return NULL;
    }

    return table_data->table;
}

// 插入数据
bool row_engine_insert(StorageEngine* engine, const char* table_name, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查是否需要扩展表容量
    if (table_data->row_count >= table_data->capacity) {
        size_t new_capacity = table_data->capacity * 2;
        if (!row_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 分配行ID
    uint64_t row_id = table_data->next_row_id++;

    // 设置行版本
    row->version = table_data->transaction_id;

    // 插入行
    table_data->rows[table_data->row_count] = row;
    table_data->row_count++;

    // 更新表的行数
    table_data->table->row_count = table_data->row_count;

    return true;
}

// 批量插入数据
bool row_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count) {
    if (!engine || !table_name || !rows || row_count == 0) {
        return false;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查是否需要扩展表容量
    if (table_data->row_count + row_count > table_data->capacity) {
        size_t new_capacity = table_data->capacity;
        while (new_capacity < table_data->row_count + row_count) {
            new_capacity *= 2;
        }
        if (!row_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 批量插入行
    for (size_t i = 0; i < row_count; i++) {
        // 设置行版本
        rows[i]->version = table_data->transaction_id;

        // 插入行
        table_data->rows[table_data->row_count + i] = rows[i];
    }

    // 更新表数据
    table_data->row_count += row_count;
    table_data->next_row_id += row_count;
    table_data->table->row_count = table_data->row_count;

    return true;
}

// 更新数据
bool row_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return false;
    }

    // 查找行
    size_t row_index = (size_t)-1;
    for (size_t i = 0; i < table_data->row_count; i++) {
        // 假设行ID是连续的，这里简化处理
        // 实际实现中应该使用索引
        if (i + 1 == row_id) {
            row_index = i;
            break;
        }
    }

    if (row_index == (size_t)-1) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 释放旧行
    if (table_data->rows[row_index]) {
        destroy_row(table_data->rows[row_index]);
    }

    // 设置新行版本
    row->version = table_data->transaction_id;

    // 更新行
    table_data->rows[row_index] = row;

    return true;
}

// 删除数据
bool row_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return false;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return false;
    }

    // 查找行
    size_t row_index = (size_t)-1;
    for (size_t i = 0; i < table_data->row_count; i++) {
        // 假设行ID是连续的，这里简化处理
        // 实际实现中应该使用索引
        if (i + 1 == row_id) {
            row_index = i;
            break;
        }
    }

    if (row_index == (size_t)-1) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 标记行为删除状态
    if (table_data->rows[row_index]) {
        table_data->rows[row_index]->deleted = true;
        table_data->rows[row_index]->version = table_data->transaction_id;
    }

    return true;
}

// 查询数据
Row* row_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return NULL;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return NULL;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return NULL;
    }

    // 查找行
    size_t row_index = (size_t)-1;
    for (size_t i = 0; i < table_data->row_count; i++) {
        // 假设行ID是连续的，这里简化处理
        // 实际实现中应该使用索引
        if (i + 1 == row_id) {
            row_index = i;
            break;
        }
    }

    if (row_index == (size_t)-1) {
        fprintf(stderr, "Row not found\n");
        return NULL;
    }

    Row* row = table_data->rows[row_index];
    if (!row || row->deleted) {
        return NULL;
    }

    return row;
}

// 开始事务
bool row_engine_begin_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    RowEngineData* data = (RowEngineData*)engine->data;
    uint64_t transaction_id = data->next_transaction_id++;

    // 为所有表设置事务ID
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->transaction_id = transaction_id;
        data->tables[i]->in_transaction = true;
    }

    return true;
}

// 提交事务
bool row_engine_commit_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    RowEngineData* data = (RowEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;
    }

    return true;
}

// 回滚事务
bool row_engine_rollback_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    // 简化实现，实际应该恢复到事务开始前的状态
    RowEngineData* data = (RowEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;
    }

    return true;
}

// 优化表
bool row_engine_optimize(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    RowEngineTableData* table_data = row_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 压缩表，移除已删除的行
    size_t new_row_count = 0;
    for (size_t i = 0; i < table_data->row_count; i++) {
        if (!table_data->rows[i]->deleted) {
            if (new_row_count != i) {
                table_data->rows[new_row_count] = table_data->rows[i];
            }
            new_row_count++;
        } else {
            destroy_row(table_data->rows[i]);
            table_data->rows[i] = NULL;
        }
    }

    // 调整表容量
    if (new_row_count < table_data->capacity / 2) {
        size_t new_capacity = table_data->capacity / 2;
        if (new_capacity < 1024) {
            new_capacity = 1024;
        }

        Row** new_rows = (Row**)realloc(table_data->rows, sizeof(Row*) * new_capacity);
        if (new_rows) {
            table_data->rows = new_rows;
            table_data->capacity = new_capacity;
        }
    }

    table_data->row_count = new_row_count;
    table_data->table->row_count = new_row_count;

    return true;
}

// 执行检查点
bool row_engine_checkpoint(StorageEngine* engine) {
    // 简化实现，实际应该将内存中的数据持久化到磁盘
    return true;
}

// 销毁引擎
void row_engine_destroy(StorageEngine* engine) {
    if (!engine) {
        return;
    }

    RowEngineData* data = (RowEngineData*)engine->data;

    // 销毁所有表
    for (size_t i = 0; i < data->table_count; i++) {
        if (data->tables[i]->rows) {
            for (size_t j = 0; j < data->tables[i]->row_count; j++) {
                if (data->tables[i]->rows[j]) {
                    destroy_row(data->tables[i]->rows[j]);
                }
            }
            free(data->tables[i]->rows);
        }
        free(data->tables[i]);
    }

    if (data->tables) {
        free(data->tables);
    }

    free(data);
    free(engine);
}