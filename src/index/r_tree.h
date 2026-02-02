#ifndef R_TREE_H
#define R_TREE_H

#include <stdint.h>
#include <stdbool.h>

// 空间维度
#define R_TREE_DIMENSIONS 2

// 节点最大子节点数
#define R_TREE_MAX_CHILDREN 8

// 空间边界框结构
typedef struct {
    double min[R_TREE_DIMENSIONS];
    double max[R_TREE_DIMENSIONS];
} r_tree_bbox;

// R树节点结构
typedef struct r_tree_node {
    bool is_leaf;
    uint32_t count;
    r_tree_bbox bbox;
    union {
        struct {
            char *key;
            uint32_t key_size;
            char *value;
            uint32_t value_size;
            r_tree_bbox item_bbox;
        } leaves[R_TREE_MAX_CHILDREN];
        struct {
            struct r_tree_node *child;
            r_tree_bbox child_bbox;
        } internal[R_TREE_MAX_CHILDREN];
    } data;
} r_tree_node;

// R树结构
typedef struct {
    r_tree_node *root;
    uint32_t height;
    uint32_t item_count;
} r_tree;

// 初始化R树
r_tree *r_tree_create(void);

// 销毁R树
void r_tree_destroy(r_tree *tree);

// 插入空间数据
bool r_tree_insert(r_tree *tree, const char *key, uint32_t key_size, const char *value, uint32_t value_size, const r_tree_bbox *bbox);

// 查询点是否在空间范围内
char **r_tree_query(r_tree *tree, const r_tree_bbox *query_bbox, uint32_t *result_count, uint32_t *value_sizes);

// 删除空间数据
bool r_tree_delete(r_tree *tree, const char *key, uint32_t key_size);

// 获取R树大小
uint32_t r_tree_size(r_tree *tree);

#endif // R_TREE_H
