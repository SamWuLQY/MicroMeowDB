#include "metadata.h"
#include "../config/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

// 确保目录存在
static bool ensure_directory(const char* dir) {
    struct stat st;
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Failed to create directory: %s\n", dir);
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Path is not a directory: %s\n", dir);
        return false;
    }
    return true;
}

// 生成元数据文件路径
static char* generate_metadata_path(const char* metadata_dir, int type, const char* schema, const char* name) {
    char* path = (char*)malloc(512);
    if (!path) {
        return NULL;
    }
    
    const char* type_dir = NULL;
    switch (type) {
        case METADATA_TYPE_DATABASE: type_dir = "databases"; break;
        case METADATA_TYPE_TABLE: type_dir = "tables"; break;
        case METADATA_TYPE_COLUMN: type_dir = "columns"; break;
        case METADATA_TYPE_INDEX: type_dir = "indexes"; break;
        case METADATA_TYPE_VIEW: type_dir = "views"; break;
        case METADATA_TYPE_TRIGGER: type_dir = "triggers"; break;
        case METADATA_TYPE_PROCEDURE: type_dir = "procedures"; break;
        case METADATA_TYPE_FUNCTION: type_dir = "functions"; break;
        case METADATA_TYPE_USER: type_dir = "users"; break;
        case METADATA_TYPE_ROLE: type_dir = "roles"; break;
        case METADATA_TYPE_SCHEMA: type_dir = "schemas"; break;
        default: type_dir = "other";
    }
    
    if (schema && strcmp(schema, "") != 0) {
        snprintf(path, 512, "%s/%s/%s/%s", metadata_dir, type_dir, schema, name);
    } else {
        snprintf(path, 512, "%s/%s/%s", metadata_dir, type_dir, name);
    }
    
    return path;
}

// 创建元数据对象
static Metadata* create_metadata_object(const char* name, const char* schema, int type, int status, void* data) {
    Metadata* metadata = (Metadata*)malloc(sizeof(Metadata));
    if (!metadata) {
        return NULL;
    }
    
    uint64_t now = (uint64_t)time(NULL);
    
    metadata->name = strdup(name);
    metadata->schema = schema ? strdup(schema) : NULL;
    metadata->type = type;
    metadata->status = status;
    metadata->created_at = now;
    metadata->updated_at = now;
    metadata->created_by = strdup("system");
    metadata->updated_by = strdup("system");
    metadata->data = data;
    
    return metadata;
}

// 销毁元数据对象
static void destroy_metadata_object(Metadata* metadata) {
    if (!metadata) {
        return;
    }
    
    if (metadata->name) {
        free(metadata->name);
    }
    if (metadata->schema) {
        free(metadata->schema);
    }
    if (metadata->created_by) {
        free(metadata->created_by);
    }
    if (metadata->updated_by) {
        free(metadata->updated_by);
    }
    if (metadata->data) {
        // 根据元数据类型销毁具体数据
        switch (metadata->type) {
            case METADATA_TYPE_TABLE:
                {
                    TableMetadata* table = (TableMetadata*)metadata->data;
                    if (table->name) free(table->name);
                    if (table->schema) free(table->schema);
                    if (table->engine) free(table->engine);
                    if (table->charset) free(table->charset);
                    if (table->collation) free(table->collation);
                    if (table->comment) free(table->comment);
                    free(table);
                }
                break;
            case METADATA_TYPE_COLUMN:
                {
                    ColumnMetadata* column = (ColumnMetadata*)metadata->data;
                    if (column->name) free(column->name);
                    if (column->table_name) free(column->table_name);
                    if (column->schema) free(column->schema);
                    if (column->data_type) free(column->data_type);
                    if (column->default_value) free(column->default_value);
                    if (column->comment) free(column->comment);
                    free(column);
                }
                break;
            case METADATA_TYPE_INDEX:
                {
                    IndexMetadata* index = (IndexMetadata*)metadata->data;
                    if (index->name) free(index->name);
                    if (index->table_name) free(index->table_name);
                    if (index->schema) free(index->schema);
                    if (index->type) free(index->type);
                    if (index->columns) {
                        for (size_t i = 0; i < index->column_count; i++) {
                            if (index->columns[i]) {
                                free(index->columns[i]);
                            }
                        }
                        free(index->columns);
                    }
                    if (index->comment) free(index->comment);
                    free(index);
                }
                break;
            default:
                free(metadata->data);
        }
    }
    
    free(metadata);
}

// 序列化表元数据
static bool serialize_table_metadata(TableMetadata* table, FILE* fp) {
    if (!table || !fp) {
        return false;
    }
    
    fprintf(fp, "name=%s\n", table->name);
    fprintf(fp, "schema=%s\n", table->schema ? table->schema : "");
    fprintf(fp, "engine=%s\n", table->engine ? table->engine : "");
    fprintf(fp, "charset=%s\n", table->charset ? table->charset : "");
    fprintf(fp, "collation=%s\n", table->collation ? table->collation : "");
    fprintf(fp, "row_format=%d\n", table->row_format);
    fprintf(fp, "auto_increment=%d\n", table->auto_increment);
    fprintf(fp, "row_count=%llu\n", table->row_count);
    fprintf(fp, "data_size=%llu\n", table->data_size);
    fprintf(fp, "index_size=%llu\n", table->index_size);
    fprintf(fp, "comment=%s\n", table->comment ? table->comment : "");
    
    return true;
}

// 反序列化表元数据
static TableMetadata* deserialize_table_metadata(FILE* fp) {
    if (!fp) {
        return NULL;
    }
    
    TableMetadata* table = (TableMetadata*)malloc(sizeof(TableMetadata));
    if (!table) {
        return NULL;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (!key || !value) {
            continue;
        }
        
        if (strcmp(key, "name") == 0) {
            table->name = strdup(value);
        } else if (strcmp(key, "schema") == 0) {
            table->schema = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "engine") == 0) {
            table->engine = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "charset") == 0) {
            table->charset = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "collation") == 0) {
            table->collation = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "row_format") == 0) {
            table->row_format = atoi(value);
        } else if (strcmp(key, "auto_increment") == 0) {
            table->auto_increment = atoi(value);
        } else if (strcmp(key, "row_count") == 0) {
            table->row_count = strtoull(value, NULL, 10);
        } else if (strcmp(key, "data_size") == 0) {
            table->data_size = strtoull(value, NULL, 10);
        } else if (strcmp(key, "index_size") == 0) {
            table->index_size = strtoull(value, NULL, 10);
        } else if (strcmp(key, "comment") == 0) {
            table->comment = strcmp(value, "") == 0 ? NULL : strdup(value);
        }
    }
    
    return table;
}

// 序列化列元数据
static bool serialize_column_metadata(ColumnMetadata* column, FILE* fp) {
    if (!column || !fp) {
        return false;
    }
    
    fprintf(fp, "name=%s\n", column->name);
    fprintf(fp, "table_name=%s\n", column->table_name);
    fprintf(fp, "schema=%s\n", column->schema ? column->schema : "");
    fprintf(fp, "data_type=%s\n", column->data_type);
    fprintf(fp, "length=%zu\n", column->length);
    fprintf(fp, "nullable=%d\n", column->nullable);
    fprintf(fp, "primary_key=%d\n", column->primary_key);
    fprintf(fp, "unique=%d\n", column->unique);
    fprintf(fp, "auto_increment=%d\n", column->auto_increment);
    fprintf(fp, "default_value=%s\n", column->default_value ? (char*)column->default_value : "");
    fprintf(fp, "comment=%s\n", column->comment ? column->comment : "");
    fprintf(fp, "position=%d\n", column->position);
    
    return true;
}

// 反序列化列元数据
static ColumnMetadata* deserialize_column_metadata(FILE* fp) {
    if (!fp) {
        return NULL;
    }
    
    ColumnMetadata* column = (ColumnMetadata*)malloc(sizeof(ColumnMetadata));
    if (!column) {
        return NULL;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (!key || !value) {
            continue;
        }
        
        if (strcmp(key, "name") == 0) {
            column->name = strdup(value);
        } else if (strcmp(key, "table_name") == 0) {
            column->table_name = strdup(value);
        } else if (strcmp(key, "schema") == 0) {
            column->schema = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "data_type") == 0) {
            column->data_type = strdup(value);
        } else if (strcmp(key, "length") == 0) {
            column->length = strtoull(value, NULL, 10);
        } else if (strcmp(key, "nullable") == 0) {
            column->nullable = atoi(value);
        } else if (strcmp(key, "primary_key") == 0) {
            column->primary_key = atoi(value);
        } else if (strcmp(key, "unique") == 0) {
            column->unique = atoi(value);
        } else if (strcmp(key, "auto_increment") == 0) {
            column->auto_increment = atoi(value);
        } else if (strcmp(key, "default_value") == 0) {
            column->default_value = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "comment") == 0) {
            column->comment = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "position") == 0) {
            column->position = atoi(value);
        }
    }
    
    return column;
}

// 序列化索引元数据
static bool serialize_index_metadata(IndexMetadata* index, FILE* fp) {
    if (!index || !fp) {
        return false;
    }
    
    fprintf(fp, "name=%s\n", index->name);
    fprintf(fp, "table_name=%s\n", index->table_name);
    fprintf(fp, "schema=%s\n", index->schema ? index->schema : "");
    fprintf(fp, "type=%s\n", index->type);
    fprintf(fp, "unique=%d\n", index->unique);
    fprintf(fp, "primary=%d\n", index->primary);
    fprintf(fp, "column_count=%zu\n", index->column_count);
    for (size_t i = 0; i < index->column_count; i++) {
        fprintf(fp, "column%d=%s\n", (int)i, index->columns[i]);
    }
    fprintf(fp, "comment=%s\n", index->comment ? index->comment : "");
    
    return true;
}

// 反序列化索引元数据
static IndexMetadata* deserialize_index_metadata(FILE* fp) {
    if (!fp) {
        return NULL;
    }
    
    IndexMetadata* index = (IndexMetadata*)malloc(sizeof(IndexMetadata));
    if (!index) {
        return NULL;
    }
    
    index->columns = NULL;
    index->column_count = 0;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (!key || !value) {
            continue;
        }
        
        if (strcmp(key, "name") == 0) {
            index->name = strdup(value);
        } else if (strcmp(key, "table_name") == 0) {
            index->table_name = strdup(value);
        } else if (strcmp(key, "schema") == 0) {
            index->schema = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "type") == 0) {
            index->type = strdup(value);
        } else if (strcmp(key, "unique") == 0) {
            index->unique = atoi(value);
        } else if (strcmp(key, "primary") == 0) {
            index->primary = atoi(value);
        } else if (strcmp(key, "column_count") == 0) {
            index->column_count = strtoull(value, NULL, 10);
            index->columns = (char**)malloc(index->column_count * sizeof(char*));
        } else if (strncmp(key, "column", 6) == 0) {
            int pos = atoi(key + 6);
            if (pos >= 0 && pos < (int)index->column_count) {
                index->columns[pos] = strdup(value);
            }
        } else if (strcmp(key, "comment") == 0) {
            index->comment = strcmp(value, "") == 0 ? NULL : strdup(value);
        }
    }
    
    return index;
}

// 序列化元数据对象
static bool serialize_metadata(Metadata* metadata, const char* path) {
    if (!metadata || !path) {
        return false;
    }
    
    // 确保目录存在
    char* dir = strdup(path);
    if (!dir) {
        return false;
    }
    
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!ensure_directory(dir)) {
            free(dir);
            return false;
        }
    }
    free(dir);
    
    // 打开文件
    FILE* fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open metadata file for writing: %s\n", path);
        return false;
    }
    
    // 写入元数据头
    fprintf(fp, "name=%s\n", metadata->name);
    fprintf(fp, "schema=%s\n", metadata->schema ? metadata->schema : "");
    fprintf(fp, "type=%d\n", metadata->type);
    fprintf(fp, "status=%d\n", metadata->status);
    fprintf(fp, "created_at=%llu\n", metadata->created_at);
    fprintf(fp, "updated_at=%llu\n", metadata->updated_at);
    fprintf(fp, "created_by=%s\n", metadata->created_by);
    fprintf(fp, "updated_by=%s\n", metadata->updated_by);
    fprintf(fp, "data_type=%d\n", metadata->type);
    fprintf(fp, "\n");
    
    // 写入具体数据
    switch (metadata->type) {
        case METADATA_TYPE_TABLE:
            serialize_table_metadata((TableMetadata*)metadata->data, fp);
            break;
        case METADATA_TYPE_COLUMN:
            serialize_column_metadata((ColumnMetadata*)metadata->data, fp);
            break;
        case METADATA_TYPE_INDEX:
            serialize_index_metadata((IndexMetadata*)metadata->data, fp);
            break;
        default:
            // 其他类型的序列化
            break;
    }
    
    fclose(fp);
    return true;
}

// 反序列化元数据对象
static Metadata* deserialize_metadata(const char* path) {
    if (!path) {
        return NULL;
    }
    
    // 打开文件
    FILE* fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open metadata file for reading: %s\n", path);
        return NULL;
    }
    
    // 读取元数据头
    char* name = NULL;
    char* schema = NULL;
    int type = -1;
    int status = OBJECT_STATUS_ACTIVE;
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
    char* created_by = NULL;
    char* updated_by = NULL;
    int data_type = -1;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strcmp(line, "\n") == 0) {
            break;
        }
        
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (!key || !value) {
            continue;
        }
        
        if (strcmp(key, "name") == 0) {
            name = strdup(value);
        } else if (strcmp(key, "schema") == 0) {
            schema = strcmp(value, "") == 0 ? NULL : strdup(value);
        } else if (strcmp(key, "type") == 0) {
            type = atoi(value);
        } else if (strcmp(key, "status") == 0) {
            status = atoi(value);
        } else if (strcmp(key, "created_at") == 0) {
            created_at = strtoull(value, NULL, 10);
        } else if (strcmp(key, "updated_at") == 0) {
            updated_at = strtoull(value, NULL, 10);
        } else if (strcmp(key, "created_by") == 0) {
            created_by = strdup(value);
        } else if (strcmp(key, "updated_by") == 0) {
            updated_by = strdup(value);
        } else if (strcmp(key, "data_type") == 0) {
            data_type = atoi(value);
        }
    }
    
    if (type == -1 || !name) {
        fclose(fp);
        if (name) free(name);
        if (schema) free(schema);
        if (created_by) free(created_by);
        if (updated_by) free(updated_by);
        return NULL;
    }
    
    // 读取具体数据
    void* data = NULL;
    switch (type) {
        case METADATA_TYPE_TABLE:
            data = deserialize_table_metadata(fp);
            break;
        case METADATA_TYPE_COLUMN:
            data = deserialize_column_metadata(fp);
            break;
        case METADATA_TYPE_INDEX:
            data = deserialize_index_metadata(fp);
            break;
        default:
            // 其他类型的反序列化
            break;
    }
    
    fclose(fp);
    
    // 创建元数据对象
    Metadata* metadata = (Metadata*)malloc(sizeof(Metadata));
    if (!metadata) {
        if (name) free(name);
        if (schema) free(schema);
        if (created_by) free(created_by);
        if (updated_by) free(updated_by);
        if (data) free(data);
        return NULL;
    }
    
    metadata->name = name;
    metadata->schema = schema;
    metadata->type = type;
    metadata->status = status;
    metadata->created_at = created_at;
    metadata->updated_at = updated_at;
    metadata->created_by = created_by;
    metadata->updated_by = updated_by;
    metadata->data = data;
    
    return metadata;
}

// 初始化元数据管理器
MetadataManager* metadata_manager_init(const char* metadata_dir) {
    MetadataManager* manager = (MetadataManager*)malloc(sizeof(MetadataManager));
    if (!manager) {
        return NULL;
    }
    
    manager->metadata_dir = metadata_dir ? strdup(metadata_dir) : strdup("./metadata");
    manager->objects = NULL;
    manager->object_count = 0;
    manager->dirty = false;
    
    // 确保元数据目录存在
    if (!ensure_directory(manager->metadata_dir)) {
        free(manager->metadata_dir);
        free(manager);
        return NULL;
    }
    
    // 加载元数据
    metadata_load(manager);
    
    return manager;
}

// 销毁元数据管理器
void metadata_manager_destroy(MetadataManager* manager) {
    if (!manager) {
        return;
    }
    
    // 保存未持久化的更改
    if (manager->dirty) {
        metadata_save(manager);
    }
    
    // 销毁所有元数据对象
    for (size_t i = 0; i < manager->object_count; i++) {
        destroy_metadata_object(manager->objects[i]);
    }
    if (manager->objects) {
        free(manager->objects);
    }
    
    if (manager->metadata_dir) {
        free(manager->metadata_dir);
    }
    
    free(manager);
}

// 加载元数据
bool metadata_load(MetadataManager* manager) {
    if (!manager || !manager->metadata_dir) {
        return false;
    }
    
    // 扫描元数据目录
    // 这里简化处理，实际需要扫描所有子目录
    
    return true;
}

// 保存元数据
bool metadata_save(MetadataManager* manager) {
    if (!manager || !manager->metadata_dir) {
        return false;
    }
    
    // 保存所有元数据对象
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->status != OBJECT_STATUS_DELETED) {
            char* path = generate_metadata_path(manager->metadata_dir, metadata->type, metadata->schema, metadata->name);
            if (path) {
                serialize_metadata(metadata, path);
                free(path);
            }
        }
    }
    
    manager->dirty = false;
    return true;
}

// 创建元数据对象
Metadata* metadata_create(MetadataManager* manager, const char* name, const char* schema, int type, void* data) {
    if (!manager || !name) {
        return NULL;
    }
    
    // 检查是否已存在
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* existing = manager->objects[i];
        if (existing->type == type && strcmp(existing->name, name) == 0 && 
            ((!schema && !existing->schema) || (schema && existing->schema && strcmp(schema, existing->schema) == 0))) {
            return NULL;
        }
    }
    
    // 创建元数据对象
    Metadata* metadata = create_metadata_object(name, schema, type, OBJECT_STATUS_ACTIVE, data);
    if (!metadata) {
        return NULL;
    }
    
    // 添加到管理器
    manager->objects = (Metadata**)realloc(manager->objects, (manager->object_count + 1) * sizeof(Metadata*));
    if (!manager->objects) {
        destroy_metadata_object(metadata);
        return NULL;
    }
    
    manager->objects[manager->object_count] = metadata;
    manager->object_count++;
    manager->dirty = true;
    
    return metadata;
}

// 获取元数据对象
Metadata* metadata_get(MetadataManager* manager, const char* name, const char* schema, int type) {
    if (!manager || !name) {
        return NULL;
    }
    
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == type && metadata->status == OBJECT_STATUS_ACTIVE && 
            strcmp(metadata->name, name) == 0 && 
            ((!schema && !metadata->schema) || (schema && metadata->schema && strcmp(schema, metadata->schema) == 0))) {
            return metadata;
        }
    }
    
    return NULL;
}

// 更新元数据对象
bool metadata_update(MetadataManager* manager, const char* name, const char* schema, int type, void* data) {
    if (!manager || !name) {
        return false;
    }
    
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == type && metadata->status == OBJECT_STATUS_ACTIVE && 
            strcmp(metadata->name, name) == 0 && 
            ((!schema && !metadata->schema) || (schema && metadata->schema && strcmp(schema, metadata->schema) == 0))) {
            
            // 释放旧数据
            if (metadata->data) {
                switch (type) {
                    case METADATA_TYPE_TABLE:
                        {
                            TableMetadata* table = (TableMetadata*)metadata->data;
                            if (table->name) free(table->name);
                            if (table->schema) free(table->schema);
                            if (table->engine) free(table->engine);
                            if (table->charset) free(table->charset);
                            if (table->collation) free(table->collation);
                            if (table->comment) free(table->comment);
                            free(table);
                        }
                        break;
                    case METADATA_TYPE_COLUMN:
                        {
                            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
                            if (column->name) free(column->name);
                            if (column->table_name) free(column->table_name);
                            if (column->schema) free(column->schema);
                            if (column->data_type) free(column->data_type);
                            if (column->default_value) free(column->default_value);
                            if (column->comment) free(column->comment);
                            free(column);
                        }
                        break;
                    case METADATA_TYPE_INDEX:
                        {
                            IndexMetadata* index = (IndexMetadata*)metadata->data;
                            if (index->name) free(index->name);
                            if (index->table_name) free(index->table_name);
                            if (index->schema) free(index->schema);
                            if (index->type) free(index->type);
                            if (index->columns) {
                                for (size_t j = 0; j < index->column_count; j++) {
                                    if (index->columns[j]) {
                                        free(index->columns[j]);
                                    }
                                }
                                free(index->columns);
                            }
                            if (index->comment) free(index->comment);
                            free(index);
                        }
                        break;
                    default:
                        free(metadata->data);
                }
            }
            
            // 更新数据
            metadata->data = data;
            metadata->updated_at = (uint64_t)time(NULL);
            if (metadata->updated_by) {
                free(metadata->updated_by);
            }
            metadata->updated_by = strdup("system");
            manager->dirty = true;
            
            return true;
        }
    }
    
    return false;
}

// 删除元数据对象
bool metadata_delete(MetadataManager* manager, const char* name, const char* schema, int type) {
    if (!manager || !name) {
        return false;
    }
    
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == type && metadata->status == OBJECT_STATUS_ACTIVE && 
            strcmp(metadata->name, name) == 0 && 
            ((!schema && !metadata->schema) || (schema && metadata->schema && strcmp(schema, metadata->schema) == 0))) {
            
            metadata->status = OBJECT_STATUS_DELETED;
            metadata->updated_at = (uint64_t)time(NULL);
            if (metadata->updated_by) {
                free(metadata->updated_by);
            }
            metadata->updated_by = strdup("system");
            manager->dirty = true;
            
            // 删除文件
            char* path = generate_metadata_path(manager->metadata_dir, type, schema, name);
            if (path) {
                remove(path);
                free(path);
            }
            
            return true;
        }
    }
    
    return false;
}

// 列出指定类型的元数据对象
Metadata** metadata_list(MetadataManager* manager, int type, size_t* count) {
    if (!manager || !count) {
        return NULL;
    }
    
    size_t matching_count = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        if (manager->objects[i]->type == type && manager->objects[i]->status == OBJECT_STATUS_ACTIVE) {
            matching_count++;
        }
    }
    
    Metadata** result = (Metadata**)malloc(matching_count * sizeof(Metadata*));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    size_t index = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        if (manager->objects[i]->type == type && manager->objects[i]->status == OBJECT_STATUS_ACTIVE) {
            result[index++] = manager->objects[i];
        }
    }
    
    *count = matching_count;
    return result;
}

// 列出表的所有列
ColumnMetadata** metadata_list_columns(MetadataManager* manager, const char* table_name, const char* schema, size_t* count) {
    if (!manager || !table_name || !count) {
        return NULL;
    }
    
    size_t matching_count = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                matching_count++;
            }
        }
    }
    
    ColumnMetadata** result = (ColumnMetadata**)malloc(matching_count * sizeof(ColumnMetadata*));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    size_t index = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                result[index++] = column;
            }
        }
    }
    
    *count = matching_count;
    return result;
}

// 列出表的所有索引
IndexMetadata** metadata_list_indexes(MetadataManager* manager, const char* table_name, const char* schema, size_t* count) {
    if (!manager || !table_name || !count) {
        return NULL;
    }
    
    size_t matching_count = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                matching_count++;
            }
        }
    }
    
    IndexMetadata** result = (IndexMetadata**)malloc(matching_count * sizeof(IndexMetadata*));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    size_t index = 0;
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                result[index++] = index;
            }
        }
    }
    
    *count = matching_count;
    return result;
}

// 检查表是否存在
bool metadata_table_exists(MetadataManager* manager, const char* table_name, const char* schema) {
    return metadata_get(manager, table_name, schema, METADATA_TYPE_TABLE) != NULL;
}

// 检查列是否存在
bool metadata_column_exists(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema) {
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->name, column_name) == 0 && 
                strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                return true;
            }
        }
    }
    return false;
}

// 检查索引是否存在
bool metadata_index_exists(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema) {
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->name, index_name) == 0 && 
                strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                return true;
            }
        }
    }
    return false;
}

// 创建表元数据
bool metadata_create_table(MetadataManager* manager, TableMetadata* table) {
    if (!manager || !table || !table->name) {
        return false;
    }
    
    // 创建表元数据
    Metadata* metadata = metadata_create(manager, table->name, table->schema, METADATA_TYPE_TABLE, table);
    if (!metadata) {
        return false;
    }
    
    // 保存到文件
    char* path = generate_metadata_path(manager->metadata_dir, METADATA_TYPE_TABLE, table->schema, table->name);
    if (path) {
        serialize_metadata(metadata, path);
        free(path);
    }
    
    return true;
}

// 创建列元数据
bool metadata_create_column(MetadataManager* manager, ColumnMetadata* column) {
    if (!manager || !column || !column->name || !column->table_name) {
        return false;
    }
    
    // 创建列元数据
    Metadata* metadata = metadata_create(manager, column->name, column->schema, METADATA_TYPE_COLUMN, column);
    if (!metadata) {
        return false;
    }
    
    // 保存到文件
    char* path = generate_metadata_path(manager->metadata_dir, METADATA_TYPE_COLUMN, column->schema, column->name);
    if (path) {
        serialize_metadata(metadata, path);
        free(path);
    }
    
    return true;
}

// 创建索引元数据
bool metadata_create_index(MetadataManager* manager, IndexMetadata* index) {
    if (!manager || !index || !index->name || !index->table_name) {
        return false;
    }
    
    // 创建索引元数据
    Metadata* metadata = metadata_create(manager, index->name, index->schema, METADATA_TYPE_INDEX, index);
    if (!metadata) {
        return false;
    }
    
    // 保存到文件
    char* path = generate_metadata_path(manager->metadata_dir, METADATA_TYPE_INDEX, index->schema, index->name);
    if (path) {
        serialize_metadata(metadata, path);
        free(path);
    }
    
    return true;
}

// 删除表元数据（包括其列和索引）
bool metadata_drop_table(MetadataManager* manager, const char* table_name, const char* schema) {
    if (!manager || !table_name) {
        return false;
    }
    
    // 删除表的所有列
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                metadata_delete(manager, column->name, schema, METADATA_TYPE_COLUMN);
            }
        }
    }
    
    // 删除表的所有索引
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                metadata_delete(manager, index->name, schema, METADATA_TYPE_INDEX);
            }
        }
    }
    
    // 删除表本身
    return metadata_delete(manager, table_name, schema, METADATA_TYPE_TABLE);
}

// 删除列元数据
bool metadata_drop_column(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema) {
    if (!manager || !column_name || !table_name) {
        return false;
    }
    
    // 查找列
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->name, column_name) == 0 && 
                strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                return metadata_delete(manager, column_name, schema, METADATA_TYPE_COLUMN);
            }
        }
    }
    
    return false;
}

// 删除索引元数据
bool metadata_drop_index(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema) {
    if (!manager || !index_name || !table_name) {
        return false;
    }
    
    // 查找索引
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->name, index_name) == 0 && 
                strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                return metadata_delete(manager, index_name, schema, METADATA_TYPE_INDEX);
            }
        }
    }
    
    return false;
}

// 获取表元数据
TableMetadata* metadata_get_table(MetadataManager* manager, const char* table_name, const char* schema) {
    Metadata* metadata = metadata_get(manager, table_name, schema, METADATA_TYPE_TABLE);
    return metadata ? (TableMetadata*)metadata->data : NULL;
}

// 获取列元数据
ColumnMetadata* metadata_get_column(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema) {
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->name, column_name) == 0 && 
                strcmp(column->table_name, table_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                return column;
            }
        }
    }
    return NULL;
}

// 获取索引元数据
IndexMetadata* metadata_get_index(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema) {
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->name, index_name) == 0 && 
                strcmp(index->table_name, table_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                return index;
            }
        }
    }
    return NULL;
}

// 更新表元数据
bool metadata_update_table(MetadataManager* manager, TableMetadata* table) {
    if (!manager || !table || !table->name) {
        return false;
    }
    
    return metadata_update(manager, table->name, table->schema, METADATA_TYPE_TABLE, table);
}

// 更新列元数据
bool metadata_update_column(MetadataManager* manager, ColumnMetadata* column) {
    if (!manager || !column || !column->name || !column->table_name) {
        return false;
    }
    
    return metadata_update(manager, column->name, column->schema, METADATA_TYPE_COLUMN, column);
}

// 更新索引元数据
bool metadata_update_index(MetadataManager* manager, IndexMetadata* index) {
    if (!manager || !index || !index->name || !index->table_name) {
        return false;
    }
    
    return metadata_update(manager, index->name, index->schema, METADATA_TYPE_INDEX, index);
}

// 克隆表结构
bool metadata_clone_table(MetadataManager* manager, const char* source_table, const char* target_table, const char* schema) {
    if (!manager || !source_table || !target_table) {
        return false;
    }
    
    // 获取源表元数据
    TableMetadata* source_meta = metadata_get_table(manager, source_table, schema);
    if (!source_meta) {
        return false;
    }
    
    // 创建目标表元数据
    TableMetadata* target_meta = (TableMetadata*)malloc(sizeof(TableMetadata));
    if (!target_meta) {
        return false;
    }
    
    target_meta->name = strdup(target_table);
    target_meta->schema = source_meta->schema ? strdup(source_meta->schema) : NULL;
    target_meta->engine = source_meta->engine ? strdup(source_meta->engine) : NULL;
    target_meta->charset = source_meta->charset ? strdup(source_meta->charset) : NULL;
    target_meta->collation = source_meta->collation ? strdup(source_meta->collation) : NULL;
    target_meta->row_format = source_meta->row_format;
    target_meta->auto_increment = source_meta->auto_increment;
    target_meta->row_count = 0;
    target_meta->data_size = 0;
    target_meta->index_size = 0;
    target_meta->comment = source_meta->comment ? strdup(source_meta->comment) : NULL;
    
    // 创建目标表
    if (!metadata_create_table(manager, target_meta)) {
        free(target_meta->name);
        if (target_meta->schema) free(target_meta->schema);
        if (target_meta->engine) free(target_meta->engine);
        if (target_meta->charset) free(target_meta->charset);
        if (target_meta->collation) free(target_meta->collation);
        if (target_meta->comment) free(target_meta->comment);
        free(target_meta);
        return false;
    }
    
    // 克隆列
    size_t column_count;
    ColumnMetadata** columns = metadata_list_columns(manager, source_table, schema, &column_count);
    if (columns) {
        for (size_t i = 0; i < column_count; i++) {
            ColumnMetadata* source_column = columns[i];
            ColumnMetadata* target_column = (ColumnMetadata*)malloc(sizeof(ColumnMetadata));
            if (target_column) {
                target_column->name = strdup(source_column->name);
                target_column->table_name = strdup(target_table);
                target_column->schema = source_column->schema ? strdup(source_column->schema) : NULL;
                target_column->data_type = strdup(source_column->data_type);
                target_column->length = source_column->length;
                target_column->nullable = source_column->nullable;
                target_column->primary_key = source_column->primary_key;
                target_column->unique = source_column->unique;
                target_column->auto_increment = source_column->auto_increment;
                target_column->default_value = source_column->default_value ? strdup((char*)source_column->default_value) : NULL;
                target_column->comment = source_column->comment ? strdup(source_column->comment) : NULL;
                target_column->position = source_column->position;
                
                metadata_create_column(manager, target_column);
            }
        }
        free(columns);
    }
    
    // 克隆索引
    size_t index_count;
    IndexMetadata** indexes = metadata_list_indexes(manager, source_table, schema, &index_count);
    if (indexes) {
        for (size_t i = 0; i < index_count; i++) {
            IndexMetadata* source_index = indexes[i];
            IndexMetadata* target_index = (IndexMetadata*)malloc(sizeof(IndexMetadata));
            if (target_index) {
                char* index_name = (char*)malloc(strlen(source_index->name) + strlen("_clone") + 1);
                sprintf(index_name, "%s_clone", source_index->name);
                target_index->name = index_name;
                target_index->table_name = strdup(target_table);
                target_index->schema = source_index->schema ? strdup(source_index->schema) : NULL;
                target_index->type = strdup(source_index->type);
                target_index->unique = source_index->unique;
                target_index->primary = source_index->primary;
                target_index->column_count = source_index->column_count;
                target_index->columns = (char**)malloc(target_index->column_count * sizeof(char*));
                for (size_t j = 0; j < target_index->column_count; j++) {
                    target_index->columns[j] = strdup(source_index->columns[j]);
                }
                target_index->comment = source_index->comment ? strdup(source_index->comment) : NULL;
                
                metadata_create_index(manager, target_index);
            }
        }
        free(indexes);
    }
    
    return true;
}

// 重命名表
bool metadata_rename_table(MetadataManager* manager, const char* old_name, const char* new_name, const char* schema) {
    if (!manager || !old_name || !new_name) {
        return false;
    }
    
    // 获取表元数据
    TableMetadata* table = metadata_get_table(manager, old_name, schema);
    if (!table) {
        return false;
    }
    
    // 更新表名
    free(table->name);
    table->name = strdup(new_name);
    
    // 更新元数据
    if (!metadata_update_table(manager, table)) {
        return false;
    }
    
    // 更新所有列的表名
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_COLUMN && metadata->status == OBJECT_STATUS_ACTIVE) {
            ColumnMetadata* column = (ColumnMetadata*)metadata->data;
            if (strcmp(column->table_name, old_name) == 0 && 
                ((!schema && !column->schema) || (schema && column->schema && strcmp(schema, column->schema) == 0))) {
                free(column->table_name);
                column->table_name = strdup(new_name);
                metadata_update_column(manager, column);
            }
        }
    }
    
    // 更新所有索引的表名
    for (size_t i = 0; i < manager->object_count; i++) {
        Metadata* metadata = manager->objects[i];
        if (metadata->type == METADATA_TYPE_INDEX && metadata->status == OBJECT_STATUS_ACTIVE) {
            IndexMetadata* index = (IndexMetadata*)metadata->data;
            if (strcmp(index->table_name, old_name) == 0 && 
                ((!schema && !index->schema) || (schema && index->schema && strcmp(schema, index->schema) == 0))) {
                free(index->table_name);
                index->table_name = strdup(new_name);
                metadata_update_index(manager, index);
            }
        }
    }
    
    return true;
}

// 重命名列
bool metadata_rename_column(MetadataManager* manager, const char* table_name, const char* old_name, const char* new_name, const char* schema) {
    if (!manager || !table_name || !old_name || !new_name) {
        return false;
    }
    
    // 获取列元数据
    ColumnMetadata* column = metadata_get_column(manager, old_name, table_name, schema);
    if (!column) {
        return false;
    }
    
    // 更新列名
    free(column->name);
    column->name = strdup(new_name);
    
    // 更新元数据
    return metadata_update_column(manager, column);
}

// 重命名索引
bool metadata_rename_index(MetadataManager* manager, const char* table_name, const char* old_name, const char* new_name, const char* schema) {
    if (!manager || !table_name || !old_name || !new_name) {
        return false;
    }
    
    // 获取索引元数据
    IndexMetadata* index = metadata_get_index(manager, old_name, table_name, schema);
    if (!index) {
        return false;
    }
    
    // 更新索引名
    free(index->name);
    index->name = strdup(new_name);
    
    // 更新元数据
    return metadata_update_index(manager, index);
}
