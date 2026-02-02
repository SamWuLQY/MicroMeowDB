#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 元数据类型
#define METADATA_TYPE_DATABASE 0  // 数据库
#define METADATA_TYPE_TABLE 1     // 表
#define METADATA_TYPE_COLUMN 2    // 列
#define METADATA_TYPE_INDEX 3     // 索引
#define METADATA_TYPE_VIEW 4      // 视图
#define METADATA_TYPE_TRIGGER 5   // 触发器
#define METADATA_TYPE_PROCEDURE 6 // 存储过程
#define METADATA_TYPE_FUNCTION 7  // 函数
#define METADATA_TYPE_USER 8      // 用户
#define METADATA_TYPE_ROLE 9      // 角色
#define METADATA_TYPE_SCHEMA 10   // 模式

// 数据库对象状态
#define OBJECT_STATUS_ACTIVE 0    // 活跃
#define OBJECT_STATUS_DROPPED 1   // 已删除
#define OBJECT_STATUS_INACTIVE 2  // 非活跃

// 元数据结构
typedef struct {
    char* name;           // 对象名称
    char* schema;         // 所属模式
    int type;             // 元数据类型
    int status;           // 对象状态
    uint64_t created_at;  // 创建时间
    uint64_t updated_at;  // 更新时间
    char* created_by;     // 创建者
    char* updated_by;     // 更新者
    void* data;           // 具体对象数据
} Metadata;

// 表元数据结构
typedef struct {
    char* name;           // 表名
    char* schema;         // 所属模式
    char* engine;         // 存储引擎
    char* charset;        // 字符集
    char* collation;      // 校对规则
    bool row_format;      // 行格式
    bool auto_increment;  // 自增
    uint64_t row_count;   // 行数
    uint64_t data_size;   // 数据大小
    uint64_t index_size;  // 索引大小
    char* comment;        // 注释
} TableMetadata;

// 列元数据结构
typedef struct {
    char* name;           // 列名
    char* table_name;     // 表名
    char* schema;         // 所属模式
    char* data_type;      // 数据类型
    size_t length;        // 长度
    bool nullable;        // 是否可为空
    bool primary_key;     // 是否为主键
    bool unique;          // 是否唯一
    bool auto_increment;  // 是否自增
    void* default_value;  // 默认值
    char* comment;        // 注释
    int position;         // 位置
} ColumnMetadata;

// 索引元数据结构
typedef struct {
    char* name;           // 索引名
    char* table_name;     // 表名
    char* schema;         // 所属模式
    char* type;           // 索引类型
    bool unique;          // 是否唯一
    bool primary;         // 是否为主键
    char** columns;       // 索引列
    size_t column_count;  // 索引列数
    char* comment;        // 注释
} IndexMetadata;

// 元数据管理器结构
typedef struct {
    Metadata** objects;   // 元数据对象列表
    size_t object_count;  // 元数据对象数量
    char* metadata_dir;   // 元数据存储目录
    bool dirty;           // 是否有未持久化的更改
} MetadataManager;

// 初始化元数据管理器
MetadataManager* metadata_manager_init(const char* metadata_dir);

// 销毁元数据管理器
void metadata_manager_destroy(MetadataManager* manager);

// 加载元数据
bool metadata_load(MetadataManager* manager);

// 保存元数据
bool metadata_save(MetadataManager* manager);

// 创建元数据对象
Metadata* metadata_create(MetadataManager* manager, const char* name, const char* schema, int type, void* data);

// 获取元数据对象
Metadata* metadata_get(MetadataManager* manager, const char* name, const char* schema, int type);

// 更新元数据对象
bool metadata_update(MetadataManager* manager, const char* name, const char* schema, int type, void* data);

// 删除元数据对象
bool metadata_delete(MetadataManager* manager, const char* name, const char* schema, int type);

// 列出指定类型的元数据对象
Metadata** metadata_list(MetadataManager* manager, int type, size_t* count);

// 列出表的所有列
ColumnMetadata** metadata_list_columns(MetadataManager* manager, const char* table_name, const char* schema, size_t* count);

// 列出表的所有索引
IndexMetadata** metadata_list_indexes(MetadataManager* manager, const char* table_name, const char* schema, size_t* count);

// 检查表是否存在
bool metadata_table_exists(MetadataManager* manager, const char* table_name, const char* schema);

// 检查列是否存在
bool metadata_column_exists(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema);

// 检查索引是否存在
bool metadata_index_exists(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema);

// 创建表元数据
bool metadata_create_table(MetadataManager* manager, TableMetadata* table);

// 创建列元数据
bool metadata_create_column(MetadataManager* manager, ColumnMetadata* column);

// 创建索引元数据
bool metadata_create_index(MetadataManager* manager, IndexMetadata* index);

// 删除表元数据（包括其列和索引）
bool metadata_drop_table(MetadataManager* manager, const char* table_name, const char* schema);

// 删除列元数据
bool metadata_drop_column(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema);

// 删除索引元数据
bool metadata_drop_index(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema);

// 获取表元数据
TableMetadata* metadata_get_table(MetadataManager* manager, const char* table_name, const char* schema);

// 获取列元数据
ColumnMetadata* metadata_get_column(MetadataManager* manager, const char* column_name, const char* table_name, const char* schema);

// 获取索引元数据
IndexMetadata* metadata_get_index(MetadataManager* manager, const char* index_name, const char* table_name, const char* schema);

// 更新表元数据
bool metadata_update_table(MetadataManager* manager, TableMetadata* table);

// 更新列元数据
bool metadata_update_column(MetadataManager* manager, ColumnMetadata* column);

// 更新索引元数据
bool metadata_update_index(MetadataManager* manager, IndexMetadata* index);

// 克隆表结构
bool metadata_clone_table(MetadataManager* manager, const char* source_table, const char* target_table, const char* schema);

// 重命名表
bool metadata_rename_table(MetadataManager* manager, const char* old_name, const char* new_name, const char* schema);

// 重命名列
bool metadata_rename_column(MetadataManager* manager, const char* table_name, const char* old_name, const char* new_name, const char* schema);

// 重命名索引
bool metadata_rename_index(MetadataManager* manager, const char* table_name, const char* old_name, const char* new_name, const char* schema);

#endif // METADATA_H