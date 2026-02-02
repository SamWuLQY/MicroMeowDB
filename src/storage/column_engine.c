#include "column_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 创建列存引擎
StorageEngine* create_column_engine(void* config) {
    StorageEngine* engine = (StorageEngine*)malloc(sizeof(StorageEngine));
    if (!engine) {
        return NULL;
    }

    ColumnEngineData* data = (ColumnEngineData*)malloc(sizeof(ColumnEngineData));
    if (!data) {
        free(engine);
        return NULL;
    }

    data->tables = NULL;
    data->table_count = 0;
    data->next_transaction_id = 1;

    engine->type = STORAGE_ENGINE_COLUMN;
    engine->name = "column_engine";
    engine->data = data;

    // 设置函数指针
    engine->create_table = column_engine_create_table;
    engine->drop_table = column_engine_drop_table;
    engine->get_table = column_engine_get_table;
    engine->insert = column_engine_insert;
    engine->update = column_engine_update;
    engine->delete = column_engine_delete;
    engine->select = column_engine_select;
    engine->batch_insert = column_engine_batch_insert;
    engine->begin_transaction = column_engine_begin_transaction;
    engine->commit_transaction = column_engine_commit_transaction;
    engine->rollback_transaction = column_engine_rollback_transaction;
    engine->optimize = column_engine_optimize;
    engine->checkpoint = column_engine_checkpoint;
    engine->destroy = column_engine_destroy;

    return engine;
}

// 获取列存引擎表数据
ColumnEngineTableData* column_engine_get_table_data(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return NULL;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;
    for (size_t i = 0; i < data->table_count; i++) {
        if (strcmp(data->tables[i]->table->name, table_name) == 0) {
            return data->tables[i];
        }
    }

    return NULL;
}

// 扩展列容量
bool column_engine_expand_column(ColumnEngineColumnData* column_data, size_t new_capacity) {
    if (!column_data || new_capacity <= column_data->capacity) {
        return false;
    }

    void** new_values = (void**)realloc(column_data->values, sizeof(void*) * new_capacity);
    if (!new_values) {
        return false;
    }

    bool* new_null_mask = (bool*)realloc(column_data->null_mask, sizeof(bool) * new_capacity);
    if (!new_null_mask) {
        free(new_values);
        return false;
    }

    // 初始化新分配的空间
    for (size_t i = column_data->capacity; i < new_capacity; i++) {
        new_values[i] = NULL;
        new_null_mask[i] = false;
    }

    column_data->values = new_values;
    column_data->null_mask = new_null_mask;
    column_data->capacity = new_capacity;

    return true;
}

// 扩展表容量
bool column_engine_expand_table(StorageEngine* engine, ColumnEngineTableData* table_data, size_t new_capacity) {
    if (!engine || !table_data || new_capacity <= table_data->capacity) {
        return false;
    }

    // 扩展所有列的容量
    for (size_t i = 0; i < table_data->column_count; i++) {
        if (!column_engine_expand_column(table_data->columns[i], new_capacity)) {
            return false;
        }
    }

    table_data->capacity = new_capacity;
    return true;
}

// 创建表
bool column_engine_create_table(StorageEngine* engine, Table* table) {
    if (!engine || !table) {
        return false;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;

    // 检查表是否已存在
    if (column_engine_get_table_data(engine, table->name)) {
        fprintf(stderr, "Table already exists\n");
        return false;
    }

    // 创建表数据结构
    ColumnEngineTableData* table_data = (ColumnEngineTableData*)malloc(sizeof(ColumnEngineTableData));
    if (!table_data) {
        return false;
    }

    // 初始化表数据
    table_data->table = table;
    table_data->column_count = table->column_count;
    table_data->row_count = 0;
    table_data->capacity = 1024; // 默认容量
    table_data->next_row_id = 1;
    table_data->transaction_id = 0;
    table_data->in_transaction = false;

    // 创建列数据结构
    table_data->columns = (ColumnEngineColumnData**)malloc(sizeof(ColumnEngineColumnData*) * table->column_count);
    if (!table_data->columns) {
        free(table_data);
        return false;
    }

    // 初始化列数据
    for (size_t i = 0; i < table->column_count; i++) {
        ColumnEngineColumnData* column_data = (ColumnEngineColumnData*)malloc(sizeof(ColumnEngineColumnData));
        if (!column_data) {
            // 释放已分配的列
            for (size_t j = 0; j < i; j++) {
                free(table_data->columns[j]->values);
                free(table_data->columns[j]->null_mask);
                free(table_data->columns[j]);
            }
            free(table_data->columns);
            free(table_data);
            return false;
        }

        column_data->column = &table->columns[i];
        column_data->value_count = 0;
        column_data->capacity = table_data->capacity;

        // 分配值数组
        column_data->values = (void**)malloc(sizeof(void*) * column_data->capacity);
        if (!column_data->values) {
            free(column_data);
            // 释放已分配的列
            for (size_t j = 0; j < i; j++) {
                free(table_data->columns[j]->values);
                free(table_data->columns[j]->null_mask);
                free(table_data->columns[j]);
            }
            free(table_data->columns);
            free(table_data);
            return false;
        }

        // 分配空值掩码
        column_data->null_mask = (bool*)malloc(sizeof(bool) * column_data->capacity);
        if (!column_data->null_mask) {
            free(column_data->values);
            free(column_data);
            // 释放已分配的列
            for (size_t j = 0; j < i; j++) {
                free(table_data->columns[j]->values);
                free(table_data->columns[j]->null_mask);
                free(table_data->columns[j]);
            }
            free(table_data->columns);
            free(table_data);
            return false;
        }

        // 初始化值数组和空值掩码
        for (size_t j = 0; j < column_data->capacity; j++) {
            column_data->values[j] = NULL;
            column_data->null_mask[j] = false;
        }

        table_data->columns[i] = column_data;
    }

    // 将表数据添加到引擎
    ColumnEngineTableData** new_tables = (ColumnEngineTableData**)realloc(data->tables, sizeof(ColumnEngineTableData*) * (data->table_count + 1));
    if (!new_tables) {
        // 释放表数据
        for (size_t i = 0; i < table_data->column_count; i++) {
            free(table_data->columns[i]->values);
            free(table_data->columns[i]->null_mask);
            free(table_data->columns[i]);
        }
        free(table_data->columns);
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
bool column_engine_drop_table(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;

    // 查找表
    size_t table_index = (size_t)-1;
    ColumnEngineTableData* table_data = NULL;
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
    for (size_t i = 0; i < table_data->column_count; i++) {
        if (table_data->columns[i]->values) {
            for (size_t j = 0; j < table_data->row_count; j++) {
                if (table_data->columns[i]->values[j]) {
                    free(table_data->columns[i]->values[j]);
                }
            }
            free(table_data->columns[i]->values);
        }
        if (table_data->columns[i]->null_mask) {
            free(table_data->columns[i]->null_mask);
        }
        free(table_data->columns[i]);
    }
    free(table_data->columns);
    free(table_data);

    // 从引擎中移除表
    for (size_t i = table_index; i < data->table_count - 1; i++) {
        data->tables[i] = data->tables[i + 1];
    }

    ColumnEngineTableData** new_tables = (ColumnEngineTableData**)realloc(data->tables, sizeof(ColumnEngineTableData*) * (data->table_count - 1));
    if (new_tables || data->table_count == 1) {
        data->tables = new_tables;
    }

    data->table_count--;

    return true;
}

// 获取表
Table* column_engine_get_table(StorageEngine* engine, const char* table_name) {
    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        return NULL;
    }

    return table_data->table;
}

// 插入数据
bool column_engine_insert(StorageEngine* engine, const char* table_name, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查是否需要扩展表容量
    if (table_data->row_count >= table_data->capacity) {
        size_t new_capacity = table_data->capacity * 2;
        if (!column_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 分配行ID
    uint64_t row_id = table_data->next_row_id++;

    // 插入行数据到每列
    for (size_t i = 0; i < table_data->column_count; i++) {
        ColumnEngineColumnData* column_data = table_data->columns[i];
        void* value = row->values[i];

        if (value) {
            // 复制值
            size_t value_size = 0;
            switch (column_data->column->data_type) {
                case DATA_TYPE_INT:
                    value_size = sizeof(int);
                    break;
                case DATA_TYPE_BIGINT:
                    value_size = sizeof(int64_t);
                    break;
                case DATA_TYPE_FLOAT:
                    value_size = sizeof(float);
                    break;
                case DATA_TYPE_DOUBLE:
                    value_size = sizeof(double);
                    break;
                case DATA_TYPE_CHAR:
                case DATA_TYPE_VARCHAR:
                    value_size = strlen((const char*)value) + 1;
                    break;
                case DATA_TYPE_DATE:
                case DATA_TYPE_DATETIME:
                    value_size = sizeof(time_t);
                    break;
                case DATA_TYPE_BOOLEAN:
                    value_size = sizeof(bool);
                    break;
                case DATA_TYPE_BLOB:
                    // 假设BLOB值是一个包含大小和数据的结构
                    // 这里简化处理
                    value_size = 1024;
                    break;
                default:
                    value_size = 64;
                    break;
            }

            void* copied_value = malloc(value_size);
            if (!copied_value) {
                return false;
            }
            memcpy(copied_value, value, value_size);

            column_data->values[table_data->row_count] = copied_value;
            column_data->null_mask[table_data->row_count] = false;
        } else {
            column_data->values[table_data->row_count] = NULL;
            column_data->null_mask[table_data->row_count] = true;
        }

        column_data->value_count++;
    }

    table_data->row_count++;
    table_data->table->row_count = table_data->row_count;

    return true;
}

// 批量插入数据
bool column_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count) {
    if (!engine || !table_name || !rows || row_count == 0) {
        return false;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
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
        if (!column_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 批量插入行数据
    for (size_t r = 0; r < row_count; r++) {
        Row* row = rows[r];

        // 插入行数据到每列
        for (size_t i = 0; i < table_data->column_count; i++) {
            ColumnEngineColumnData* column_data = table_data->columns[i];
            void* value = row->values[i];

            if (value) {
                // 复制值
                size_t value_size = 0;
                switch (column_data->column->data_type) {
                    case DATA_TYPE_INT:
                        value_size = sizeof(int);
                        break;
                    case DATA_TYPE_BIGINT:
                        value_size = sizeof(int64_t);
                        break;
                    case DATA_TYPE_FLOAT:
                        value_size = sizeof(float);
                        break;
                    case DATA_TYPE_DOUBLE:
                        value_size = sizeof(double);
                        break;
                    case DATA_TYPE_CHAR:
                    case DATA_TYPE_VARCHAR:
                        value_size = strlen((const char*)value) + 1;
                        break;
                    case DATA_TYPE_DATE:
                    case DATA_TYPE_DATETIME:
                        value_size = sizeof(time_t);
                        break;
                    case DATA_TYPE_BOOLEAN:
                        value_size = sizeof(bool);
                        break;
                    case DATA_TYPE_BLOB:
                        value_size = 1024;
                        break;
                    default:
                        value_size = 64;
                        break;
                }

                void* copied_value = malloc(value_size);
                if (!copied_value) {
                    return false;
                }
                memcpy(copied_value, value, value_size);

                column_data->values[table_data->row_count + r] = copied_value;
                column_data->null_mask[table_data->row_count + r] = false;
            } else {
                column_data->values[table_data->row_count + r] = NULL;
                column_data->null_mask[table_data->row_count + r] = true;
            }

            column_data->value_count++;
        }
    }

    table_data->row_count += row_count;
    table_data->next_row_id += row_count;
    table_data->table->row_count = table_data->row_count;

    return true;
}

// 更新数据
bool column_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return false;
    }

    size_t row_index = row_id - 1;
    if (row_index >= table_data->row_count) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 更新每列的数据
    for (size_t i = 0; i < table_data->column_count; i++) {
        ColumnEngineColumnData* column_data = table_data->columns[i];
        void* value = row->values[i];

        // 释放旧值
        if (column_data->values[row_index]) {
            free(column_data->values[row_index]);
            column_data->values[row_index] = NULL;
        }

        if (value) {
            // 复制新值
            size_t value_size = 0;
            switch (column_data->column->data_type) {
                case DATA_TYPE_INT:
                    value_size = sizeof(int);
                    break;
                case DATA_TYPE_BIGINT:
                    value_size = sizeof(int64_t);
                    break;
                case DATA_TYPE_FLOAT:
                    value_size = sizeof(float);
                    break;
                case DATA_TYPE_DOUBLE:
                    value_size = sizeof(double);
                    break;
                case DATA_TYPE_CHAR:
                case DATA_TYPE_VARCHAR:
                    value_size = strlen((const char*)value) + 1;
                    break;
                case DATA_TYPE_DATE:
                case DATA_TYPE_DATETIME:
                    value_size = sizeof(time_t);
                    break;
                case DATA_TYPE_BOOLEAN:
                    value_size = sizeof(bool);
                    break;
                case DATA_TYPE_BLOB:
                    value_size = 1024;
                    break;
                default:
                    value_size = 64;
                    break;
            }

            void* copied_value = malloc(value_size);
            if (!copied_value) {
                return false;
            }
            memcpy(copied_value, value, value_size);

            column_data->values[row_index] = copied_value;
            column_data->null_mask[row_index] = false;
        } else {
            column_data->null_mask[row_index] = true;
        }
    }

    return true;
}

// 删除数据
bool column_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return false;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return false;
    }

    size_t row_index = row_id - 1;
    if (row_index >= table_data->row_count) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 标记行为删除状态
    // 简化实现，实际应该使用删除标记或墓碑
    for (size_t i = 0; i < table_data->column_count; i++) {
        ColumnEngineColumnData* column_data = table_data->columns[i];
        if (column_data->values[row_index]) {
            free(column_data->values[row_index]);
            column_data->values[row_index] = NULL;
        }
        column_data->null_mask[row_index] = true;
    }

    return true;
}

// 查询数据
Row* column_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return NULL;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return NULL;
    }

    // 检查行ID是否有效
    if (row_id == 0 || row_id >= table_data->next_row_id) {
        fprintf(stderr, "Invalid row ID\n");
        return NULL;
    }

    size_t row_index = row_id - 1;
    if (row_index >= table_data->row_count) {
        fprintf(stderr, "Row not found\n");
        return NULL;
    }

    // 创建行数据
    Row* row = create_row(table_data->column_count);
    if (!row) {
        return NULL;
    }

    // 填充行数据
    for (size_t i = 0; i < table_data->column_count; i++) {
        ColumnEngineColumnData* column_data = table_data->columns[i];
        if (!column_data->null_mask[row_index] && column_data->values[row_index]) {
            // 复制值
            size_t value_size = 0;
            switch (column_data->column->data_type) {
                case DATA_TYPE_INT:
                    value_size = sizeof(int);
                    break;
                case DATA_TYPE_BIGINT:
                    value_size = sizeof(int64_t);
                    break;
                case DATA_TYPE_FLOAT:
                    value_size = sizeof(float);
                    break;
                case DATA_TYPE_DOUBLE:
                    value_size = sizeof(double);
                    break;
                case DATA_TYPE_CHAR:
                case DATA_TYPE_VARCHAR:
                    value_size = strlen((const char*)column_data->values[row_index]) + 1;
                    break;
                case DATA_TYPE_DATE:
                case DATA_TYPE_DATETIME:
                    value_size = sizeof(time_t);
                    break;
                case DATA_TYPE_BOOLEAN:
                    value_size = sizeof(bool);
                    break;
                case DATA_TYPE_BLOB:
                    value_size = 1024;
                    break;
                default:
                    value_size = 64;
                    break;
            }

            void* copied_value = malloc(value_size);
            if (!copied_value) {
                destroy_row(row);
                return NULL;
            }
            memcpy(copied_value, column_data->values[row_index], value_size);

            row->values[i] = copied_value;
        } else {
            row->values[i] = NULL;
        }
    }

    row->version = table_data->transaction_id;

    return row;
}

// 开始事务
bool column_engine_begin_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;
    uint64_t transaction_id = data->next_transaction_id++;

    // 为所有表设置事务ID
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->transaction_id = transaction_id;
        data->tables[i]->in_transaction = true;
    }

    return true;
}

// 提交事务
bool column_engine_commit_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;
    }

    return true;
}

// 回滚事务
bool column_engine_rollback_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    // 简化实现，实际应该恢复到事务开始前的状态
    ColumnEngineData* data = (ColumnEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;
    }

    return true;
}

// 优化表
bool column_engine_optimize(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    ColumnEngineTableData* table_data = column_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 压缩表，移除已删除的行
    size_t new_row_count = 0;
    for (size_t i = 0; i < table_data->row_count; i++) {
        bool row_deleted = true;
        for (size_t j = 0; j < table_data->column_count; j++) {
            if (!table_data->columns[j]->null_mask[i]) {
                row_deleted = false;
                break;
            }
        }

        if (!row_deleted) {
            if (new_row_count != i) {
                // 移动行数据
                for (size_t j = 0; j < table_data->column_count; j++) {
                    ColumnEngineColumnData* column_data = table_data->columns[j];
                    if (column_data->values[i]) {
                        column_data->values[new_row_count] = column_data->values[i];
                    } else {
                        column_data->values[new_row_count] = NULL;
                    }
                    column_data->null_mask[new_row_count] = column_data->null_mask[i];
                }
            }
            new_row_count++;
        } else {
            // 释放已删除行的数据
            for (size_t j = 0; j < table_data->column_count; j++) {
                if (table_data->columns[j]->values[i]) {
                    free(table_data->columns[j]->values[i]);
                    table_data->columns[j]->values[i] = NULL;
                }
            }
        }
    }

    // 调整表容量
    if (new_row_count < table_data->capacity / 2) {
        size_t new_capacity = table_data->capacity / 2;
        if (new_capacity < 1024) {
            new_capacity = 1024;
        }

        // 调整所有列的容量
        for (size_t i = 0; i < table_data->column_count; i++) {
            ColumnEngineColumnData* column_data = table_data->columns[i];
            void** new_values = (void**)realloc(column_data->values, sizeof(void*) * new_capacity);
            if (new_values) {
                column_data->values = new_values;
            }

            bool* new_null_mask = (bool*)realloc(column_data->null_mask, sizeof(bool) * new_capacity);
            if (new_null_mask) {
                column_data->null_mask = new_null_mask;
            }

            column_data->capacity = new_capacity;
            column_data->value_count = new_row_count;
        }

        table_data->capacity = new_capacity;
    }

    table_data->row_count = new_row_count;
    table_data->table->row_count = new_row_count;

    return true;
}

// 执行检查点
bool column_engine_checkpoint(StorageEngine* engine) {
    // 简化实现，实际应该将内存中的数据持久化到磁盘
    return true;
}

// 销毁引擎
void column_engine_destroy(StorageEngine* engine) {
    if (!engine) {
        return;
    }

    ColumnEngineData* data = (ColumnEngineData*)engine->data;

    // 销毁所有表
    for (size_t i = 0; i < data->table_count; i++) {
        ColumnEngineTableData* table_data = data->tables[i];

        // 销毁所有列
        for (size_t j = 0; j < table_data->column_count; j++) {
            ColumnEngineColumnData* column_data = table_data->columns[j];

            // 释放所有值
            for (size_t k = 0; k < table_data->row_count; k++) {
                if (column_data->values[k]) {
                    free(column_data->values[k]);
                }
            }

            free(column_data->values);
            free(column_data->null_mask);
            free(column_data);
        }

        free(table_data->columns);
        free(table_data);
    }

    if (data->tables) {
        free(data->tables);
    }

    free(data);
    free(engine);
}