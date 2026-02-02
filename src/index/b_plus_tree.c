#include "b_plus_tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 创建B+树节点
BPlusTreeNode* b_plus_tree_create_node(BPlusTree* tree, NodeType type) {
    BPlusTreeNode* node = (BPlusTreeNode*)malloc(sizeof(BPlusTreeNode));
    if (!node) {
        return NULL;
    }

    node->type = type;
    node->key_count = 0;
    node->capacity = tree->node_capacity;
    node->keys = (void**)malloc(sizeof(void*) * tree->node_capacity);
    if (!node->keys) {
        free(node);
        return NULL;
    }

    if (type == NODE_TYPE_INTERNAL) {
        node->ptrs.children = (BPlusTreeNode**)malloc(sizeof(BPlusTreeNode*) * (tree->node_capacity + 1));
        if (!node->ptrs.children) {
            free(node->keys);
            free(node);
            return NULL;
        }
    } else {
        node->ptrs.values = (void**)malloc(sizeof(void*) * tree->node_capacity);
        if (!node->ptrs.values) {
            free(node->keys);
            free(node);
            return NULL;
        }
    }

    node->parent = NULL;
    node->prev = NULL;
    node->next = NULL;

    return node;
}

// 销毁B+树节点
void b_plus_tree_destroy_node(BPlusTree* tree, BPlusTreeNode* node) {
    if (!node) {
        return;
    }

    // 释放键
    for (uint32_t i = 0; i < node->key_count; i++) {
        if (tree->destroy_key && node->keys[i]) {
            tree->destroy_key(node->keys[i]);
        }
    }
    free(node->keys);

    // 释放值或子节点
    if (node->type == NODE_TYPE_INTERNAL) {
        for (uint32_t i = 0; i < node->key_count + 1; i++) {
            if (node->ptrs.children[i]) {
                b_plus_tree_destroy_node(tree, node->ptrs.children[i]);
            }
        }
        free(node->ptrs.children);
    } else {
        for (uint32_t i = 0; i < node->key_count; i++) {
            if (tree->destroy_value && node->ptrs.values[i]) {
                tree->destroy_value(node->ptrs.values[i]);
            }
        }
        free(node->ptrs.values);
    }

    free(node);
}

// 检查节点是否已满
bool b_plus_tree_node_is_full(BPlusTreeNode* node) {
    return node->key_count >= node->capacity;
}

// 检查节点是否下溢
bool b_plus_tree_node_is_underflow(BPlusTreeNode* node) {
    return node->key_count < node->capacity / 2;
}

// 查找键在节点中的索引
int b_plus_tree_find_key_index(BPlusTreeNode* node, void* key, int (*compare)(const void*, const void*)) {
    int left = 0;
    int right = node->key_count - 1;
    int index = -1;

    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = compare(key, node->keys[mid]);

        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return left;
}

// 查找叶子节点
BPlusTreeNode* b_plus_tree_find_leaf(BPlusTree* tree, void* key) {
    BPlusTreeNode* node = tree->root;

    while (node->type == NODE_TYPE_INTERNAL) {
        int index = b_plus_tree_find_key_index(node, key, tree->compare);
        node = node->ptrs.children[index];
    }

    return node;
}

// 分割节点
BPlusTreeNode* b_plus_tree_split_node(BPlusTree* tree, BPlusTreeNode* node) {
    BPlusTreeNode* new_node = b_plus_tree_create_node(tree, node->type);
    if (!new_node) {
        return NULL;
    }

    uint32_t mid = node->key_count / 2;

    // 复制键到新节点
    for (uint32_t i = mid; i < node->key_count; i++) {
        new_node->keys[new_node->key_count] = node->keys[i];
        node->keys[i] = NULL;
        new_node->key_count++;
    }

    // 复制值或子节点到新节点
    if (node->type == NODE_TYPE_INTERNAL) {
        for (uint32_t i = mid; i < node->key_count + 1; i++) {
            new_node->ptrs.children[new_node->key_count] = node->ptrs.children[i];
            node->ptrs.children[i] = NULL;
            new_node->ptrs.children[new_node->key_count]->parent = new_node;
            new_node->key_count++;
        }
        new_node->key_count--; // 内部节点的子节点数比键数多1
    } else {
        for (uint32_t i = mid; i < node->key_count; i++) {
            new_node->ptrs.values[new_node->key_count] = node->ptrs.values[i];
            node->ptrs.values[i] = NULL;
            new_node->key_count++;
        }

        // 连接叶子节点
        new_node->prev = node;
        new_node->next = node->next;
        if (node->next) {
            node->next->prev = new_node;
        }
        node->next = new_node;

        // 更新树的最后叶子节点
        if (tree->last_leaf == node) {
            tree->last_leaf = new_node;
        }
    }

    node->key_count = mid;

    // 更新父节点
    if (node->parent == NULL) {
        // 创建新的根节点
        BPlusTreeNode* new_root = b_plus_tree_create_node(tree, NODE_TYPE_INTERNAL);
        if (!new_root) {
            b_plus_tree_destroy_node(tree, new_node);
            return NULL;
        }

        new_root->keys[0] = node->keys[mid];
        node->keys[mid] = NULL;
        new_root->ptrs.children[0] = node;
        new_root->ptrs.children[1] = new_node;
        new_root->key_count = 1;

        node->parent = new_root;
        new_node->parent = new_root;

        tree->root = new_root;
        tree->height++;
    } else {
        BPlusTreeNode* parent = node->parent;
        int index = b_plus_tree_find_key_index(parent, node->keys[node->key_count], tree->compare);

        // 插入键到父节点
        for (uint32_t i = parent->key_count; i > index; i--) {
            parent->keys[i] = parent->keys[i - 1];
            parent->ptrs.children[i + 1] = parent->ptrs.children[i];
        }

        parent->keys[index] = node->keys[node->key_count];
        parent->ptrs.children[index + 1] = new_node;
        parent->key_count++;
        node->keys[node->key_count] = NULL;
        new_node->parent = parent;

        // 检查父节点是否需要分裂
        if (b_plus_tree_node_is_full(parent)) {
            b_plus_tree_split_node(tree, parent);
        }
    }

    return new_node;
}

// 合并节点
BPlusTreeNode* b_plus_tree_merge_nodes(BPlusTree* tree, BPlusTreeNode* left, BPlusTreeNode* right) {
    if (!left || !right || left->parent != right->parent) {
        return NULL;
    }

    BPlusTreeNode* parent = left->parent;

    // 复制键和值到左节点
    for (uint32_t i = 0; i < right->key_count; i++) {
        left->keys[left->key_count] = right->keys[i];
        left->ptrs.values[left->key_count] = right->ptrs.values[i];
        left->key_count++;
    }

    // 更新叶子节点的连接
    left->next = right->next;
    if (right->next) {
        right->next->prev = left;
    }

    // 更新树的最后叶子节点
    if (tree->last_leaf == right) {
        tree->last_leaf = left;
    }

    // 从父节点移除键
    int index = b_plus_tree_find_key_index(parent, right->keys[0], tree->compare);
    if (index > 0) {
        index--;
    }

    for (uint32_t i = index; i < parent->key_count - 1; i++) {
        parent->keys[i] = parent->keys[i + 1];
        parent->ptrs.children[i + 1] = parent->ptrs.children[i + 2];
    }
    parent->key_count--;

    // 检查父节点是否需要合并
    if (parent->key_count == 0) {
        // 父节点是根节点
        tree->root = left;
        left->parent = NULL;
        b_plus_tree_destroy_node(tree, parent);
        tree->height--;
    } else if (b_plus_tree_node_is_underflow(parent)) {
        BPlusTreeNode* left_sibling = NULL;
        BPlusTreeNode* right_sibling = NULL;

        // 查找左兄弟或右兄弟
        int parent_index = b_plus_tree_find_key_index(parent, left->keys[0], tree->compare);
        if (parent_index > 0) {
            left_sibling = parent->ptrs.children[parent_index - 1];
        }
        if (parent_index < parent->key_count) {
            right_sibling = parent->ptrs.children[parent_index + 1];
        }

        if (left_sibling && left_sibling->key_count > left_sibling->capacity / 2) {
            // 从左兄弟借一个键
            // 简化实现，实际需要更复杂的逻辑
        } else if (right_sibling && right_sibling->key_count > right_sibling->capacity / 2) {
            // 从右兄弟借一个键
            // 简化实现，实际需要更复杂的逻辑
        } else if (left_sibling) {
            // 与左兄弟合并
            b_plus_tree_merge_nodes(tree, left_sibling, left);
        } else if (right_sibling) {
            // 与右兄弟合并
            b_plus_tree_merge_nodes(tree, left, right_sibling);
        }
    }

    b_plus_tree_destroy_node(tree, right);
    return left;
}

// 创建B+树索引
BPlusTree* b_plus_tree_create(uint32_t node_capacity, size_t key_size, size_t value_size, 
                             int (*compare)(const void*, const void*),
                             void (*destroy_key)(void*),
                             void (*destroy_value)(void*)) {
    BPlusTree* tree = (BPlusTree*)malloc(sizeof(BPlusTree));
    if (!tree) {
        return NULL;
    }

    // 创建根节点（叶子节点）
    BPlusTreeNode* root = b_plus_tree_create_node(tree, NODE_TYPE_LEAF);
    if (!root) {
        free(tree);
        return NULL;
    }

    tree->root = root;
    tree->first_leaf = root;
    tree->last_leaf = root;
    tree->height = 1;
    tree->node_capacity = node_capacity;
    tree->key_size = key_size;
    tree->value_size = value_size;
    tree->compare = compare;
    tree->destroy_key = destroy_key;
    tree->destroy_value = destroy_value;

    return tree;
}

// 销毁B+树索引
void b_plus_tree_destroy(BPlusTree* tree) {
    if (!tree) {
        return;
    }

    b_plus_tree_destroy_node(tree, tree->root);
    free(tree);
}

// 插入键值对
bool b_plus_tree_insert(BPlusTree* tree, void* key, void* value) {
    BPlusTreeNode* leaf = b_plus_tree_find_leaf(tree, key);
    int index = b_plus_tree_find_key_index(leaf, key, tree->compare);

    // 检查键是否已存在
    if (index < leaf->key_count && tree->compare(key, leaf->keys[index]) == 0) {
        // 更新值
        if (tree->destroy_value && leaf->ptrs.values[index]) {
            tree->destroy_value(leaf->ptrs.values[index]);
        }
        leaf->ptrs.values[index] = value;
        return true;
    }

    // 插入键值对
    if (b_plus_tree_node_is_full(leaf)) {
        // 分裂节点
        BPlusTreeNode* new_leaf = b_plus_tree_split_node(tree, leaf);
        if (!new_leaf) {
            return false;
        }

        // 确定插入到哪个节点
        if (tree->compare(key, leaf->keys[leaf->key_count - 1]) > 0) {
            leaf = new_leaf;
            index = 0;
        }
    }

    // 移动键和值为新键腾出空间
    for (uint32_t i = leaf->key_count; i > index; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->ptrs.values[i] = leaf->ptrs.values[i - 1];
    }

    // 插入新键值对
    leaf->keys[index] = key;
    leaf->ptrs.values[index] = value;
    leaf->key_count++;

    return true;
}

// 删除键值对
bool b_plus_tree_delete(BPlusTree* tree, void* key) {
    BPlusTreeNode* leaf = b_plus_tree_find_leaf(tree, key);
    int index = b_plus_tree_find_key_index(leaf, key, tree->compare);

    // 检查键是否存在
    if (index >= leaf->key_count || tree->compare(key, leaf->keys[index]) != 0) {
        return false;
    }

    // 释放键和值
    if (tree->destroy_key && leaf->keys[index]) {
        tree->destroy_key(leaf->keys[index]);
    }
    if (tree->destroy_value && leaf->ptrs.values[index]) {
        tree->destroy_value(leaf->ptrs.values[index]);
    }

    // 移动键和值填补空缺
    for (uint32_t i = index; i < leaf->key_count - 1; i++) {
        leaf->keys[i] = leaf->keys[i + 1];
        leaf->ptrs.values[i] = leaf->ptrs.values[i + 1];
    }
    leaf->key_count--;

    // 检查节点是否下溢
    if (leaf->key_count > 0) {
        return true;
    }

    // 处理下溢
    BPlusTreeNode* left_sibling = leaf->prev;
    BPlusTreeNode* right_sibling = leaf->next;

    if (left_sibling && left_sibling->key_count > left_sibling->capacity / 2) {
        // 从左兄弟借一个键值对
        for (uint32_t i = leaf->key_count; i > 0; i--) {
            leaf->keys[i] = leaf->keys[i - 1];
            leaf->ptrs.values[i] = leaf->ptrs.values[i - 1];
        }
        leaf->keys[0] = left_sibling->keys[left_sibling->key_count - 1];
        leaf->ptrs.values[0] = left_sibling->ptrs.values[left_sibling->key_count - 1];
        leaf->key_count++;
        left_sibling->key_count--;

        // 更新父节点的键
        BPlusTreeNode* parent = leaf->parent;
        int parent_index = b_plus_tree_find_key_index(parent, leaf->keys[0], tree->compare);
        if (tree->destroy_key && parent->keys[parent_index - 1]) {
            tree->destroy_key(parent->keys[parent_index - 1]);
        }
        parent->keys[parent_index - 1] = leaf->keys[0];
    } else if (right_sibling && right_sibling->key_count > right_sibling->capacity / 2) {
        // 从右兄弟借一个键值对
        leaf->keys[leaf->key_count] = right_sibling->keys[0];
        leaf->ptrs.values[leaf->key_count] = right_sibling->ptrs.values[0];
        leaf->key_count++;

        for (uint32_t i = 0; i < right_sibling->key_count - 1; i++) {
            right_sibling->keys[i] = right_sibling->keys[i + 1];
            right_sibling->ptrs.values[i] = right_sibling->ptrs.values[i + 1];
        }
        right_sibling->key_count--;

        // 更新父节点的键
        BPlusTreeNode* parent = leaf->parent;
        int parent_index = b_plus_tree_find_key_index(parent, right_sibling->keys[0], tree->compare);
        if (tree->destroy_key && parent->keys[parent_index]) {
            tree->destroy_key(parent->keys[parent_index]);
        }
        parent->keys[parent_index] = right_sibling->keys[0];
    } else if (left_sibling) {
        // 与左兄弟合并
        b_plus_tree_merge_nodes(tree, left_sibling, leaf);
    } else if (right_sibling) {
        // 与右兄弟合并
        b_plus_tree_merge_nodes(tree, leaf, right_sibling);
    } else {
        // 叶子节点是根节点
        // 简化实现，实际需要处理根节点的情况
    }

    return true;
}

// 查找键对应的值
void* b_plus_tree_find(BPlusTree* tree, void* key) {
    BPlusTreeNode* leaf = b_plus_tree_find_leaf(tree, key);
    int index = b_plus_tree_find_key_index(leaf, key, tree->compare);

    if (index < leaf->key_count && tree->compare(key, leaf->keys[index]) == 0) {
        return leaf->ptrs.values[index];
    }

    return NULL;
}

// 范围查询
bool b_plus_tree_range_query(BPlusTree* tree, void* start_key, void* end_key, 
                           void*** keys, void*** values, size_t* count) {
    if (!tree || !keys || !values || !count) {
        return false;
    }

    *count = 0;

    // 查找起始键所在的叶子节点
    BPlusTreeNode* leaf = b_plus_tree_find_leaf(tree, start_key);
    if (!leaf) {
        return false;
    }

    // 遍历叶子节点
    while (leaf) {
        for (uint32_t i = 0; i < leaf->key_count; i++) {
            // 检查键是否在范围内
            if (tree->compare(leaf->keys[i], start_key) >= 0 && 
                tree->compare(leaf->keys[i], end_key) <= 0) {
                // 扩展结果数组
                *keys = (void**)realloc(*keys, sizeof(void*) * (*count + 1));
                *values = (void**)realloc(*values, sizeof(void*) * (*count + 1));
                if (!*keys || !*values) {
                    // 释放已分配的内存
                    for (size_t j = 0; j < *count; j++) {
                        if (tree->destroy_key) {
                            tree->destroy_key((*keys)[j]);
                        }
                        if (tree->destroy_value) {
                            tree->destroy_value((*values)[j]);
                        }
                    }
                    free(*keys);
                    free(*values);
                    *count = 0;
                    return false;
                }

                // 复制键值对
                (*keys)[*count] = leaf->keys[i];
                (*values)[*count] = leaf->ptrs.values[i];
                (*count)++;
            } else if (tree->compare(leaf->keys[i], end_key) > 0) {
                // 超出范围，结束查询
                return true;
            }
        }

        // 移动到下一个叶子节点
        leaf = leaf->next;
    }

    return true;
}

// 获取树的高度
uint32_t b_plus_tree_height(BPlusTree* tree) {
    return tree->height;
}

// 获取键的数量
uint64_t b_plus_tree_key_count(BPlusTree* tree) {
    uint64_t count = 0;
    BPlusTreeNode* leaf = tree->first_leaf;

    while (leaf) {
        count += leaf->key_count;
        leaf = leaf->next;
    }

    return count;
}

// 打印B+树结构（用于调试）
void b_plus_tree_print(BPlusTree* tree) {
    if (!tree) {
        return;
    }

    printf("B+ Tree Height: %u\n", tree->height);
    printf("Node Capacity: %u\n", tree->node_capacity);
    printf("Key Count: %llu\n", b_plus_tree_key_count(tree));

    // 打印叶子节点
    BPlusTreeNode* leaf = tree->first_leaf;
    printf("Leaf Nodes:\n");
    while (leaf) {
        printf("Leaf node with %u keys:", leaf->key_count);
        for (uint32_t i = 0; i < leaf->key_count; i++) {
            printf(" %p", leaf->keys[i]);
        }
        printf("\n");
        leaf = leaf->next;
    }
}
