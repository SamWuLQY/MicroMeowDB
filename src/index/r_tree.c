#include "r_tree.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 创建R树节点
static r_tree_node *r_tree_node_create(bool is_leaf) {
    r_tree_node *node = (r_tree_node *)malloc(sizeof(r_tree_node));
    if (!node) {
        return NULL;
    }
    
    node->is_leaf = is_leaf;
    node->count = 0;
    
    // 初始化边界框
    for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
        node->bbox.min[i] = INFINITY;
        node->bbox.max[i] = -INFINITY;
    }
    
    return node;
}

// 销毁R树节点
static void r_tree_node_destroy(r_tree_node *node) {
    if (!node) {
        return;
    }
    
    if (node->is_leaf) {
        for (uint32_t i = 0; i < node->count; i++) {
            if (node->data.leaves[i].key) {
                free(node->data.leaves[i].key);
            }
            if (node->data.leaves[i].value) {
                free(node->data.leaves[i].value);
            }
        }
    } else {
        for (uint32_t i = 0; i < node->count; i++) {
            r_tree_node_destroy(node->data.internal[i].child);
        }
    }
    
    free(node);
}

// 计算边界框面积
static double r_tree_bbox_area(const r_tree_bbox *bbox) {
    double area = 1.0;
    for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
        area *= (bbox->max[i] - bbox->min[i]);
    }
    return area;
}

// 扩展边界框以包含另一个边界框
static void r_tree_bbox_extend(r_tree_bbox *target, const r_tree_bbox *source) {
    for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
        if (source->min[i] < target->min[i]) {
            target->min[i] = source->min[i];
        }
        if (source->max[i] > target->max[i]) {
            target->max[i] = source->max[i];
        }
    }
}

// 检查两个边界框是否相交
static bool r_tree_bbox_intersects(const r_tree_bbox *a, const r_tree_bbox *b) {
    for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
        if (a->min[i] > b->max[i] || a->max[i] < b->min[i]) {
            return false;
        }
    }
    return true;
}

// 选择最佳子节点插入
static uint32_t r_tree_choose_subtree(r_tree_node *node, const r_tree_bbox *bbox) {
    uint32_t best_index = 0;
    double min_increase = INFINITY;
    
    for (uint32_t i = 0; i < node->count; i++) {
        r_tree_bbox temp = node->data.internal[i].child_bbox;
        double original_area = r_tree_bbox_area(&temp);
        r_tree_bbox_extend(&temp, bbox);
        double new_area = r_tree_bbox_area(&temp);
        double area_increase = new_area - original_area;
        
        if (area_increase < min_increase) {
            min_increase = area_increase;
            best_index = i;
        }
    }
    
    return best_index;
}

// 分裂节点
static r_tree_node *r_tree_split_node(r_tree_node *node, const char *key, uint32_t key_size, const char *value, uint32_t value_size, const r_tree_bbox *bbox) {
    // 简化实现：创建新节点并移动一半数据
    r_tree_node *new_node = r_tree_node_create(node->is_leaf);
    if (!new_node) {
        return NULL;
    }
    
    // 移动一半数据到新节点
    uint32_t split_point = node->count / 2;
    
    if (node->is_leaf) {
        for (uint32_t i = split_point; i < node->count; i++) {
            memcpy(&new_node->data.leaves[new_node->count], &node->data.leaves[i], sizeof(node->data.leaves[0]));
            r_tree_bbox_extend(&new_node->bbox, &node->data.leaves[i].item_bbox);
            new_node->count++;
        }
        node->count = split_point;
        
        // 重新计算原节点边界框
        for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
            node->bbox.min[i] = INFINITY;
            node->bbox.max[i] = -INFINITY;
        }
        for (uint32_t i = 0; i < node->count; i++) {
            r_tree_bbox_extend(&node->bbox, &node->data.leaves[i].item_bbox);
        }
    } else {
        for (uint32_t i = split_point; i < node->count; i++) {
            memcpy(&new_node->data.internal[new_node->count], &node->data.internal[i], sizeof(node->data.internal[0]));
            r_tree_bbox_extend(&new_node->bbox, &node->data.internal[i].child_bbox);
            new_node->count++;
        }
        node->count = split_point;
        
        // 重新计算原节点边界框
        for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
            node->bbox.min[i] = INFINITY;
            node->bbox.max[i] = -INFINITY;
        }
        for (uint32_t i = 0; i < node->count; i++) {
            r_tree_bbox_extend(&node->bbox, &node->data.internal[i].child_bbox);
        }
    }
    
    return new_node;
}

// 递归插入
static bool r_tree_insert_recursive(r_tree_node **node_ptr, const char *key, uint32_t key_size, const char *value, uint32_t value_size, const r_tree_bbox *bbox, r_tree_node **split_node) {
    r_tree_node *node = *node_ptr;
    
    if (node->is_leaf) {
        // 插入到叶节点
        if (node->count < R_TREE_MAX_CHILDREN) {
            // 空间足够，直接插入
            node->data.leaves[node->count].key = (char *)malloc(key_size);
            if (!node->data.leaves[node->count].key) {
                return false;
            }
            memcpy(node->data.leaves[node->count].key, key, key_size);
            node->data.leaves[node->count].key_size = key_size;
            
            node->data.leaves[node->count].value = (char *)malloc(value_size);
            if (!node->data.leaves[node->count].value) {
                free(node->data.leaves[node->count].key);
                return false;
            }
            memcpy(node->data.leaves[node->count].value, value, value_size);
            node->data.leaves[node->count].value_size = value_size;
            
            node->data.leaves[node->count].item_bbox = *bbox;
            r_tree_bbox_extend(&node->bbox, bbox);
            node->count++;
            
            *split_node = NULL;
            return true;
        } else {
            // 叶节点已满，需要分裂
            *split_node = r_tree_split_node(node, key, key_size, value, value_size, bbox);
            if (!*split_node) {
                return false;
            }
            
            // 尝试插入到原节点或新节点
            if (node->count < R_TREE_MAX_CHILDREN) {
                node->data.leaves[node->count].key = (char *)malloc(key_size);
                if (!node->data.leaves[node->count].key) {
                    r_tree_node_destroy(*split_node);
                    return false;
                }
                memcpy(node->data.leaves[node->count].key, key, key_size);
                node->data.leaves[node->count].key_size = key_size;
                
                node->data.leaves[node->count].value = (char *)malloc(value_size);
                if (!node->data.leaves[node->count].value) {
                    free(node->data.leaves[node->count].key);
                    r_tree_node_destroy(*split_node);
                    return false;
                }
                memcpy(node->data.leaves[node->count].value, value, value_size);
                node->data.leaves[node->count].value_size = value_size;
                
                node->data.leaves[node->count].item_bbox = *bbox;
                r_tree_bbox_extend(&node->bbox, bbox);
                node->count++;
            } else if ((*split_node)->count < R_TREE_MAX_CHILDREN) {
                (*split_node)->data.leaves[(*split_node)->count].key = (char *)malloc(key_size);
                if (!(*split_node)->data.leaves[(*split_node)->count].key) {
                    r_tree_node_destroy(*split_node);
                    return false;
                }
                memcpy((*split_node)->data.leaves[(*split_node)->count].key, key, key_size);
                (*split_node)->data.leaves[(*split_node)->count].key_size = key_size;
                
                (*split_node)->data.leaves[(*split_node)->count].value = (char *)malloc(value_size);
                if (!(*split_node)->data.leaves[(*split_node)->count].value) {
                    free((*split_node)->data.leaves[(*split_node)->count].key);
                    r_tree_node_destroy(*split_node);
                    return false;
                }
                memcpy((*split_node)->data.leaves[(*split_node)->count].value, value, value_size);
                (*split_node)->data.leaves[(*split_node)->count].value_size = value_size;
                
                (*split_node)->data.leaves[(*split_node)->count].item_bbox = *bbox;
                r_tree_bbox_extend(&(*split_node)->bbox, bbox);
                (*split_node)->count++;
            } else {
                r_tree_node_destroy(*split_node);
                return false;
            }
            
            return true;
        }
    } else {
        // 内部节点，递归插入
        uint32_t child_index = r_tree_choose_subtree(node, bbox);
        r_tree_node *split_child = NULL;
        
        if (!r_tree_insert_recursive(&node->data.internal[child_index].child, key, key_size, value, value_size, bbox, &split_child)) {
            return false;
        }
        
        if (split_child) {
            // 子节点分裂，需要将新节点添加到当前节点
            if (node->count < R_TREE_MAX_CHILDREN) {
                node->data.internal[node->count].child = split_child;
                node->data.internal[node->count].child_bbox = split_child->bbox;
                r_tree_bbox_extend(&node->bbox, &split_child->bbox);
                node->count++;
                *split_node = NULL;
            } else {
                // 当前节点也已满，需要分裂
                *split_node = r_tree_split_node(node, NULL, 0, NULL, 0, NULL);
                if (!*split_node) {
                    r_tree_node_destroy(split_child);
                    return false;
                }
                
                // 添加新子节点
                if (node->count < R_TREE_MAX_CHILDREN) {
                    node->data.internal[node->count].child = split_child;
                    node->data.internal[node->count].child_bbox = split_child->bbox;
                    r_tree_bbox_extend(&node->bbox, &split_child->bbox);
                    node->count++;
                } else if ((*split_node)->count < R_TREE_MAX_CHILDREN) {
                    (*split_node)->data.internal[(*split_node)->count].child = split_child;
                    (*split_node)->data.internal[(*split_node)->count].child_bbox = split_child->bbox;
                    r_tree_bbox_extend(&(*split_node)->bbox, &split_child->bbox);
                    (*split_node)->count++;
                } else {
                    r_tree_node_destroy(split_child);
                    r_tree_node_destroy(*split_node);
                    return false;
                }
            }
        } else {
            // 更新子节点边界框
            node->data.internal[child_index].child_bbox = node->data.internal[child_index].child->bbox;
            // 重新计算当前节点边界框
            for (int i = 0; i < R_TREE_DIMENSIONS; i++) {
                node->bbox.min[i] = INFINITY;
                node->bbox.max[i] = -INFINITY;
            }
            for (uint32_t i = 0; i < node->count; i++) {
                r_tree_bbox_extend(&node->bbox, &node->data.internal[i].child_bbox);
            }
            *split_node = NULL;
        }
        
        return true;
    }
}

// 递归查询
static void r_tree_query_recursive(r_tree_node *node, const r_tree_bbox *query_bbox, char ***results, uint32_t *result_count, uint32_t *capacity, uint32_t **value_sizes) {
    if (!node) {
        return;
    }
    
    // 检查节点边界框是否与查询边界框相交
    if (!r_tree_bbox_intersects(&node->bbox, query_bbox)) {
        return;
    }
    
    if (node->is_leaf) {
        for (uint32_t i = 0; i < node->count; i++) {
            if (r_tree_bbox_intersects(&node->data.leaves[i].item_bbox, query_bbox)) {
                // 扩展结果数组
                if (*result_count >= *capacity) {
                    *capacity *= 2;
                    *results = (char **)realloc(*results, *capacity * sizeof(char *));
                    *value_sizes = (uint32_t *)realloc(*value_sizes, *capacity * sizeof(uint32_t));
                }
                
                // 复制值
                (*results)[*result_count] = (char *)malloc(node->data.leaves[i].value_size);
                if ((*results)[*result_count]) {
                    memcpy((*results)[*result_count], node->data.leaves[i].value, node->data.leaves[i].value_size);
                    (*value_sizes)[*result_count] = node->data.leaves[i].value_size;
                    (*result_count)++;
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < node->count; i++) {
            r_tree_query_recursive(node->data.internal[i].child, query_bbox, results, result_count, capacity, value_sizes);
        }
    }
}

// 初始化R树
r_tree *r_tree_create(void) {
    r_tree *tree = (r_tree *)malloc(sizeof(r_tree));
    if (!tree) {
        return NULL;
    }
    
    tree->root = r_tree_node_create(true);
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    
    tree->height = 1;
    tree->item_count = 0;
    
    return tree;
}

// 销毁R树
void r_tree_destroy(r_tree *tree) {
    if (tree) {
        if (tree->root) {
            r_tree_node_destroy(tree->root);
        }
        free(tree);
    }
}

// 插入空间数据
bool r_tree_insert(r_tree *tree, const char *key, uint32_t key_size, const char *value, uint32_t value_size, const r_tree_bbox *bbox) {
    if (!tree || !key || !value || !bbox) {
        return false;
    }
    
    r_tree_node *split_node = NULL;
    if (!r_tree_insert_recursive(&tree->root, key, key_size, value, value_size, bbox, &split_node)) {
        return false;
    }
    
    // 如果根节点分裂，创建新根节点
    if (split_node) {
        r_tree_node *new_root = r_tree_node_create(false);
        if (!new_root) {
            r_tree_node_destroy(split_node);
            return false;
        }
        
        new_root->data.internal[0].child = tree->root;
        new_root->data.internal[0].child_bbox = tree->root->bbox;
        new_root->data.internal[1].child = split_node;
        new_root->data.internal[1].child_bbox = split_node->bbox;
        new_root->count = 2;
        
        // 计算新根节点边界框
        new_root->bbox = new_root->data.internal[0].child_bbox;
        r_tree_bbox_extend(&new_root->bbox, &new_root->data.internal[1].child_bbox);
        
        tree->root = new_root;
        tree->height++;
    }
    
    tree->item_count++;
    return true;
}

// 查询点是否在空间范围内
char **r_tree_query(r_tree *tree, const r_tree_bbox *query_bbox, uint32_t *result_count, uint32_t *value_sizes) {
    if (!tree || !query_bbox || !result_count) {
        return NULL;
    }
    
    *result_count = 0;
    uint32_t capacity = 16;
    char **results = (char **)malloc(capacity * sizeof(char *));
    if (!results) {
        return NULL;
    }
    
    if (value_sizes) {
        *value_sizes = (uint32_t *)malloc(capacity * sizeof(uint32_t));
        if (!*value_sizes) {
            free(results);
            return NULL;
        }
    }
    
    r_tree_query_recursive(tree->root, query_bbox, &results, result_count, &capacity, value_sizes);
    
    // 收缩结果数组
    if (*result_count > 0) {
        results = (char **)realloc(results, *result_count * sizeof(char *));
        if (value_sizes) {
            *value_sizes = (uint32_t *)realloc(*value_sizes, *result_count * sizeof(uint32_t));
        }
    } else {
        free(results);
        results = NULL;
        if (value_sizes) {
            free(*value_sizes);
            *value_sizes = NULL;
        }
    }
    
    return results;
}

// 删除空间数据
bool r_tree_delete(r_tree *tree, const char *key, uint32_t key_size) {
    // 简化实现：暂不支持删除
    (void)tree;
    (void)key;
    (void)key_size;
    return false;
}

// 获取R树大小
uint32_t r_tree_size(r_tree *tree) {
    if (!tree) {
        return 0;
    }
    return tree->item_count;
}
