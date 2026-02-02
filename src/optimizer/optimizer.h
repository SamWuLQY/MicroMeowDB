#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "../config/config.h"
#include "../error/error.h"
#include "../logging/logging.h"
#include "../metadata/metadata.h"

#define OPTIMIZER_MODULE "optimizer"

#define MAX_QUERY_LENGTH 10240
#define MAX_PLAN_NODES 1024
#define MAX_TABLES_PER_QUERY 64
#define MAX_JOINS_PER_QUERY 64
#define MAX_PREDICATES_PER_QUERY 128

#define QUERY_TYPE_SELECT 1
#define QUERY_TYPE_INSERT 2
#define QUERY_TYPE_UPDATE 3
#define QUERY_TYPE_DELETE 4
#define QUERY_TYPE_CREATE 5
#define QUERY_TYPE_DROP 6
#define QUERY_TYPE_ALTER 7
#define QUERY_TYPE_TRUNCATE 8
#define QUERY_TYPE_RENAME 9
#define QUERY_TYPE_GRANT 10
#define QUERY_TYPE_REVOKE 11
#define QUERY_TYPE_COMMIT 12
#define QUERY_TYPE_ROLLBACK 13
#define QUERY_TYPE_BEGIN 14

#define JOIN_TYPE_INNER 1
#define JOIN_TYPE_LEFT 2
#define JOIN_TYPE_RIGHT 3
#define JOIN_TYPE_FULL 4
#define JOIN_TYPE_CROSS 5

#define PREDICATE_TYPE_EQ 1
#define PREDICATE_TYPE_NE 2
#define PREDICATE_TYPE_LT 3
#define PREDICATE_TYPE_LE 4
#define PREDICATE_TYPE_GT 5
#define PREDICATE_TYPE_GE 6
#define PREDICATE_TYPE_LIKE 7
#define PREDICATE_TYPE_IN 8
#define PREDICATE_TYPE_NOT_IN 9
#define PREDICATE_TYPE_BETWEEN 10
#define PREDICATE_TYPE_IS_NULL 11
#define PREDICATE_TYPE_IS_NOT_NULL 12

#define PLAN_NODE_TYPE_SEQUENTIAL_SCAN 1
#define PLAN_NODE_TYPE_INDEX_SCAN 2
#define PLAN_NODE_TYPE_JOIN 3
#define PLAN_NODE_TYPE_FILTER 4
#define PLAN_NODE_TYPE_PROJECTION 5
#define PLAN_NODE_TYPE_SORT 6
#define PLAN_NODE_TYPE_AGGREGATE 7
#define PLAN_NODE_TYPE_LIMIT 8
#define PLAN_NODE_TYPE_OFFSET 9

#define OPTIMIZATION_LEVEL_NONE 0
#define OPTIMIZATION_LEVEL_BASIC 1
#define OPTIMIZATION_LEVEL_FULL 2

#define STATISTICS_TYPE_ROW_COUNT 1
#define STATISTICS_TYPE_CARDINALITY 2
#define STATISTICS_TYPE_MIN_VALUE 3
#define STATISTICS_TYPE_MAX_VALUE 4
#define STATISTICS_TYPE_AVERAGE_VALUE 5
#define STATISTICS_TYPE_STANDARD_DEVIATION 6

// 表达式结构
typedef struct {
    int type;
    char *value;
    struct expression *left;
    struct expression *right;
} expression;

// 谓词结构
typedef struct {
    int type;
    char *column;
    expression *expr;
} predicate;

// 表引用结构
typedef struct {
    char *name;
    char *alias;
    char *schema;
} table_ref;

// 连接结构
typedef struct {
    int type;
    table_ref *left_table;
    table_ref *right_table;
    predicate *condition;
} join;

// 查询结构
typedef struct {
    int type;
    table_ref *tables[MAX_TABLES_PER_QUERY];
    int table_count;
    char *columns[MAX_PREDICATES_PER_QUERY];
    int column_count;
    predicate *predicates[MAX_PREDICATES_PER_QUERY];
    int predicate_count;
    join *joins[MAX_JOINS_PER_QUERY];
    int join_count;
    char *group_by_columns[MAX_PREDICATES_PER_QUERY];
    int group_by_count;
    char *order_by_columns[MAX_PREDICATES_PER_QUERY];
    int order_by_count;
    int limit;
    int offset;
    char *insert_values[MAX_PREDICATES_PER_QUERY];
    int insert_value_count;
    char *update_values[MAX_PREDICATES_PER_QUERY];
    int update_value_count;
    char *create_definition;
    char *drop_target;
    char *alter_operation;
    char *truncate_target;
    char *rename_old_name;
    char *rename_new_name;
    char *grant_privileges;
    char *grant_user;
    char *revoke_privileges;
    char *revoke_user;
} query;

// 计划节点结构
typedef struct plan_node {
    int type;
    char *name;
    struct plan_node *left_child;
    struct plan_node *right_child;
    table_ref *table;
    char *index_name;
    predicate *filter;
    char *columns[MAX_PREDICATES_PER_QUERY];
    int column_count;
    char *sort_columns[MAX_PREDICATES_PER_QUERY];
    int sort_column_count;
    int limit;
    int offset;
    int join_type;
    predicate *join_condition;
    double estimated_cost;
    double estimated_rows;
    struct plan_node *next;
} plan_node;

// 查询计划结构
typedef struct {
    plan_node *root;
    double total_cost;
    double estimated_rows;
    int node_count;
    char *query_text;
} query_plan;

// 统计信息结构
typedef struct {
    char *table_name;
    char *column_name;
    int type;
    double value;
    char *string_value;
    uint64_t timestamp;
} statistics;

// 查询优化器结构
typedef struct {
    config_t *config;
    MetadataManager *metadata;
    statistics *statistics_cache;
    int statistics_count;
    int optimization_level;
    bool use_statistics;
    bool enable_join_reordering;
    bool enable_index_selection;
    bool enable_predicate_pushdown;
    bool enable_projection_pruning;
} query_optimizer;

query_optimizer *optimizer_create(config_t *config, MetadataManager *metadata);
void optimizer_destroy(query_optimizer *optimizer);

query *optimizer_parse_query(query_optimizer *optimizer, const char *query_text);
void optimizer_free_query(query *q);

query_plan *optimizer_create_plan(query_optimizer *optimizer, query *q);
void optimizer_free_plan(query_plan *plan);

query_plan *optimizer_optimize_plan(query_optimizer *optimizer, query_plan *plan);

int optimizer_execute_plan(query_optimizer *optimizer, query_plan *plan, void **result);

int optimizer_update_statistics(query_optimizer *optimizer, const char *table_name, const char *column_name, int type, double value, const char *string_value);

statistics *optimizer_get_statistics(query_optimizer *optimizer, const char *table_name, const char *column_name, int type);

void optimizer_print_plan(query_optimizer *optimizer, query_plan *plan);

#endif