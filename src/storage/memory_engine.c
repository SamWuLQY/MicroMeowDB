#include "memory_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 哈希函数
uint64_t memory_engine_hash(uint64_t row_id, size_t capacity) {
    return row_id % capacity;
}

// 创建内存表引擎
StorageEngine* create_memory_engine(void* config) {
    StorageEngine* engine = (StorageEngine*)malloc(sizeof(StorageEngine));
    if (!engine) {
        return NULL;
    }

    MemoryEngineData* data = (MemoryEngineData*)malloc(sizeof(MemoryEngineData));
    if (!data) {
        free(engine);
        return NULL;
    }

    data->tables = NULL;
    data->table_count = 0;
    data->next_transaction_id = 1;

    engine->type = STORAGE_ENGINE_MEMORY;
    engine->name = "memory_engine";
    engine->data = data;

    // 设置函数指针
    engine->create_table = memory_engine_create_table;
    engine->drop_table = memory_engine_drop_table;
    engine->get_table = memory_engine_get_table;
    engine->insert = memory_engine_insert;
    engine->update = memory_engine_update;
    engine->delete = memory_engine_delete;
    engine->select = memory_engine_select;
    engine->batch_insert = memory_engine_batch_insert;
    engine->begin_transaction = memory_engine_begin_transaction;
    engine->commit_transaction = memory_engine_commit_transaction;
    engine->rollback_transaction = memory_engine_rollback_transaction;
    engine->optimize = memory_engine_optimize;
    engine->checkpoint = memory_engine_checkpoint;
    engine->destroy = memory_engine_destroy;

    return engine;
}

// 获取内存表引擎表数据
MemoryEngineTableData* memory_engine_get_table_data(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return NULL;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;
    for (size_t i = 0; i < data->table_count; i++) {
        if (strcmp(data->tables[i]->table->name, table_name) == 0) {
            return data->tables[i];
        }
    }

    return NULL;
}

// 扩展表容量
bool memory_engine_expand_table(StorageEngine* engine, MemoryEngineTableData* table_data, size_t new_capacity) {
    if (!engine || !table_data || new_capacity <= table_data->capacity) {
        return false;
    }

    MemoryEngineRow** new_rows = (MemoryEngineRow**)malloc(sizeof(MemoryEngineRow*) * new_capacity);
    if (!new_rows) {
        return false;
    }

    // 初始化新哈希表
    for (size_t i = 0; i < new_capacity; i++) {
        new_rows[i] = NULL;
    }

    // 重新哈希所有行
    for (size_t i = 0; i < table_data->capacity; i++) {
        MemoryEngineRow* row = table_data->rows[i];
        while (row) {
            MemoryEngineRow* next = row->next;
            uint64_t new_hash = memory_engine_hash(row->row_id, new_capacity);

            // 插入到新哈希表
            row->next = new_rows[new_hash];
            if (new_rows[new_hash]) {
                new_rows[new_hash]->prev = row;
            }
            row->prev = NULL;
            new_rows[new_hash] = row;

            row = next;
        }
    }

    free(table_data->rows);
    table_data->rows = new_rows;
    table_data->capacity = new_capacity;

    return true;
}

// 创建表
bool memory_engine_create_table(StorageEngine* engine, Table* table) {
    if (!engine || !table) {
        return false;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 检查表是否已存在
    if (memory_engine_get_table_data(engine, table->name)) {
        fprintf(stderr, "Table already exists\n");
        return false;
    }

    // 创建表数据结构
    MemoryEngineTableData* table_data = (MemoryEngineTableData*)malloc(sizeof(MemoryEngineTableData));
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
    table_data->persistent = false; // 默认不持久化
    table_data->persist_file = NULL;

    // 分配哈希表
    table_data->rows = (MemoryEngineRow**)malloc(sizeof(MemoryEngineRow*) * table_data->capacity);
    if (!table_data->rows) {
        free(table_data);
        return false;
    }

    // 初始化哈希表
    for (size_t i = 0; i < table_data->capacity; i++) {
        table_data->rows[i] = NULL;
    }

    // 将表数据添加到引擎
    MemoryEngineTableData** new_tables = (MemoryEngineTableData**)realloc(data->tables, sizeof(MemoryEngineTableData*) * (data->table_count + 1));
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
bool memory_engine_drop_table(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 查找表
    size_t table_index = (size_t)-1;
    MemoryEngineTableData* table_data = NULL;
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
    for (size_t i = 0; i < table_data->capacity; i++) {
        MemoryEngineRow* row = table_data->rows[i];
        while (row) {
            MemoryEngineRow* next = row->next;
            destroy_row(row->row);
            free(row);
            row = next;
        }
    }
    free(table_data->rows);
    if (table_data->persist_file) {
        free(table_data->persist_file);
    }
    free(table_data);

    // 从引擎中移除表
    for (size_t i = table_index; i < data->table_count - 1; i++) {
        data->tables[i] = data->tables[i + 1];
    }

    MemoryEngineTableData** new_tables = (MemoryEngineTableData**)realloc(data->tables, sizeof(MemoryEngineTableData*) * (data->table_count - 1));
    if (new_tables || data->table_count == 1) {
        data->tables = new_tables;
    }

    data->table_count--;

    return true;
}

// 获取表
Table* memory_engine_get_table(StorageEngine* engine, const char* table_name) {
    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        return NULL;
    }

    return table_data->table;
}

// 插入数据
bool memory_engine_insert(StorageEngine* engine, const char* table_name, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查是否需要扩展表容量
    if (table_data->row_count >= table_data->capacity * 0.75) {
        size_t new_capacity = table_data->capacity * 2;
        if (!memory_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 分配行ID
    uint64_t row_id = table_data->next_row_id++;

    // 计算哈希值
    uint64_t hash = memory_engine_hash(row_id, table_data->capacity);

    // 创建内存行结构
    MemoryEngineRow* memory_row = (MemoryEngineRow*)malloc(sizeof(MemoryEngineRow));
    if (!memory_row) {
        return false;
    }

    // 设置行数据
    memory_row->row = row;
    memory_row->row_id = row_id;

    // 插入到哈希表
    memory_row->next = table_data->rows[hash];
    memory_row->prev = NULL;
    if (table_data->rows[hash]) {
        table_data->rows[hash]->prev = memory_row;
    }
    table_data->rows[hash] = memory_row;

    table_data->row_count++;
    table_data->table->row_count = table_data->row_count;

    // 如果需要持久化，写入持久化文件
    if (table_data->persistent && table_data->persist_file) {
        // 简化实现，实际应该实现持久化逻辑
    }

    return true;
}

// 批量插入数据
bool memory_engine_batch_insert(StorageEngine* engine, const char* table_name, Row** rows, size_t row_count) {
    if (!engine || !table_name || !rows || row_count == 0) {
        return false;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 检查是否需要扩展表容量
    if (table_data->row_count + row_count >= table_data->capacity * 0.75) {
        size_t new_capacity = table_data->capacity;
        while (table_data->row_count + row_count >= new_capacity * 0.75) {
            new_capacity *= 2;
        }
        if (!memory_engine_expand_table(engine, table_data, new_capacity)) {
            return false;
        }
    }

    // 批量插入行数据
    for (size_t i = 0; i < row_count; i++) {
        Row* row = rows[i];
        uint64_t row_id = table_data->next_row_id++;
        uint64_t hash = memory_engine_hash(row_id, table_data->capacity);

        MemoryEngineRow* memory_row = (MemoryEngineRow*)malloc(sizeof(MemoryEngineRow));
        if (!memory_row) {
            return false;
        }

        memory_row->row = row;
        memory_row->row_id = row_id;

        memory_row->next = table_data->rows[hash];
        memory_row->prev = NULL;
        if (table_data->rows[hash]) {
            table_data->rows[hash]->prev = memory_row;
        }
        table_data->rows[hash] = memory_row;
    }

    table_data->row_count += row_count;
    table_data->table->row_count = table_data->row_count;

    // 如果需要持久化，写入持久化文件
    if (table_data->persistent && table_data->persist_file) {
        // 简化实现，实际应该实现批量持久化逻辑
    }

    return true;
}

// 更新数据
bool memory_engine_update(StorageEngine* engine, const char* table_name, uint64_t row_id, Row* row) {
    if (!engine || !table_name || !row) {
        return false;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 计算哈希值
    uint64_t hash = memory_engine_hash(row_id, table_data->capacity);

    // 查找行
    MemoryEngineRow* memory_row = table_data->rows[hash];
    while (memory_row) {
        if (memory_row->row_id == row_id) {
            break;
        }
        memory_row = memory_row->next;
    }

    if (!memory_row) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 释放旧行
    destroy_row(memory_row->row);

    // 设置新行
    memory_row->row = row;

    // 如果需要持久化，写入持久化文件
    if (table_data->persistent && table_data->persist_file) {
        // 简化实现，实际应该实现持久化逻辑
    }

    return true;
}

// 删除数据
bool memory_engine_delete(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return false;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 计算哈希值
    uint64_t hash = memory_engine_hash(row_id, table_data->capacity);

    // 查找行
    MemoryEngineRow* memory_row = table_data->rows[hash];
    while (memory_row) {
        if (memory_row->row_id == row_id) {
            break;
        }
        memory_row = memory_row->next;
    }

    if (!memory_row) {
        fprintf(stderr, "Row not found\n");
        return false;
    }

    // 从哈希表中移除
    if (memory_row->prev) {
        memory_row->prev->next = memory_row->next;
    } else {
        table_data->rows[hash] = memory_row->next;
    }
    if (memory_row->next) {
        memory_row->next->prev = memory_row->prev;
    }

    // 释放行数据
    destroy_row(memory_row->row);
    free(memory_row);

    table_data->row_count--;
    table_data->table->row_count = table_data->row_count;

    // 如果需要持久化，写入持久化文件
    if (table_data->persistent && table_data->persist_file) {
        // 简化实现，实际应该实现持久化逻辑
    }

    return true;
}

// 查询数据
Row* memory_engine_select(StorageEngine* engine, const char* table_name, uint64_t row_id) {
    if (!engine || !table_name) {
        return NULL;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return NULL;
    }

    // 计算哈希值
    uint64_t hash = memory_engine_hash(row_id, table_data->capacity);

    // 查找行
    MemoryEngineRow* memory_row = table_data->rows[hash];
    while (memory_row) {
        if (memory_row->row_id == row_id) {
            break;
        }
        memory_row = memory_row->next;
    }

    if (!memory_row) {
        fprintf(stderr, "Row not found\n");
        return NULL;
    }

    // 返回行的副本
    Row* row_copy = create_row(memory_row->row->value_count);
    if (!row_copy) {
        return NULL;
    }

    for (size_t i = 0; i < row_copy->value_count; i++) {
        if (memory_row->row->values[i]) {
            // 复制值
            size_t value_size = 0;
            // 根据列类型计算值大小
            // 简化实现，实际应该根据列类型计算
            value_size = 64;
            void* copied_value = malloc(value_size);
            if (copied_value) {
                memcpy(copied_value, memory_row->row->values[i], value_size);
                row_copy->values[i] = copied_value;
            }
        } else {
            row_copy->values[i] = NULL;
        }
    }

    row_copy->version = memory_row->row->version;
    row_copy->deleted = memory_row->row->deleted;

    return row_copy;
}

// 开始事务
bool memory_engine_begin_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;
    uint64_t transaction_id = data->next_transaction_id++;

    // 为所有表设置事务ID
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->transaction_id = transaction_id;
        data->tables[i]->in_transaction = true;
    }

    return true;
}

// 提交事务
bool memory_engine_commit_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;

        // 如果需要持久化，写入持久化文件
        if (data->tables[i]->persistent && data->tables[i]->persist_file) {
            // 简化实现，实际应该实现持久化逻辑
        }
    }

    return true;
}

// 回滚事务
bool memory_engine_rollback_transaction(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    // 简化实现，实际应该恢复到事务开始前的状态
    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 结束所有表的事务
    for (size_t i = 0; i < data->table_count; i++) {
        data->tables[i]->in_transaction = false;
    }

    return true;
}

// 优化表
bool memory_engine_optimize(StorageEngine* engine, const char* table_name) {
    if (!engine || !table_name) {
        return false;
    }

    MemoryEngineTableData* table_data = memory_engine_get_table_data(engine, table_name);
    if (!table_data) {
        fprintf(stderr, "Table not found\n");
        return false;
    }

    // 压缩表，调整哈希表容量
    if (table_data->row_count < table_data->capacity * 0.25) {
        size_t new_capacity = table_data->capacity / 2;
        if (new_capacity < 1024) {
            new_capacity = 1024;
        }
        memory_engine_expand_table(engine, table_data, new_capacity);
    }

    return true;
}

// 执行检查点
bool memory_engine_checkpoint(StorageEngine* engine) {
    if (!engine) {
        return false;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 将所有需要持久化的表写入持久化文件
    for (size_t i = 0; i < data->table_count; i++) {
        if (data->tables[i]->persistent && data->tables[i]->persist_file) {
            // 简化实现，实际应该实现持久化逻辑
        }
    }

    return true;
}

// 销毁引擎
void memory_engine_destroy(StorageEngine* engine) {
    if (!engine) {
        return;
    }

    MemoryEngineData* data = (MemoryEngineData*)engine->data;

    // 销毁所有表
    for (size_t i = 0; i < data->table_count; i++) {
        MemoryEngineTableData* table_data = data->tables[i];

        // 释放所有行
        for (size_t j = 0; j < table_data->capacity; j++) {
            MemoryEngineRow* memory_row = table_data->rows[j];
            while (memory_row) {
                MemoryEngineRow* next = memory_row->next;
                destroy_row(memory_row->row);
                free(memory_row);
                memory_row = next;
            }
        }

        free(table_data->rows);
        if (table_data->persist_file) {
            free(table_data->persist_file);
        }
        free(table_data);
    }

    if (data->tables) {
        free(data->tables);
    }

    free(data);
    free(engine);
}