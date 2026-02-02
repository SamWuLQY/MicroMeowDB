#ifndef B_PLUS_TREE_H
#define B_PLUS_TREE_H

#include <stdint.h>
#include <stdbool.h>

// B+树节点类型
typedef enum {
    NODE_TYPE_INTERNAL,
    NODE_TYPE_LEAF
} NodeType;

// B+树节点结构
typedef struct {
    NodeType type;
    uint32_t key_count;
    uint32_t capacity;
    void** keys;
    union {
        struct BPlusTreeNode** children; // 内部节点
        void** values; // 叶子节点
    } ptrs;
    struct BPlusTreeNode* parent;
    struct BPlusTreeNode* prev; // 叶子节点的前驱
    struct BPlusTreeNode* next; // 叶子节点的后继
} BPlusTreeNode;

// B+树索引结构
typedef struct {
    BPlusTreeNode* root;
    BPlusTreeNode* first_leaf;
    BPlusTreeNode* last_leaf;
    uint32_t height;
    uint32_t node_capacity;
    size_t key_size;
    size_t value_size;
    int (*compare)(const void*, const void*);
    void (*destroy_key)(void*);
    void (*destroy_value)(void*);
} BPlusTree;

// 创建B+树索引
BPlusTree* b_plus_tree_create(uint32_t node_capacity, size_t key_size, size_t value_size, 
                             int (*compare)(const void*, const void*),
                             void (*destroy_key)(void*),
                             void (*destroy_value)(void*));

// 销毁B+树索引
void b_plus_tree_destroy(BPlusTree* tree);

// 插入键值对
bool b_plus_tree_insert(BPlusTree* tree, void* key, void* value);

// 删除键值对
bool b_plus_tree_delete(BPlusTree* tree, void* key);

// 查找键对应的值
void* b_plus_tree_find(BPlusTree* tree, void* key);

// 范围查询
bool b_plus_tree_range_query(BPlusTree* tree, void* start_key, void* end_key, 
                           void*** keys, void*** values, size_t* count);

// 获取树的高度
uint32_t b_plus_tree_height(BPlusTree* tree);

// 获取键的数量
uint64_t b_plus_tree_key_count(BPlusTree* tree);

// 打印B+树结构（用于调试）
void b_plus_tree_print(BPlusTree* tree);

// B+树辅助函数
BPlusTreeNode* b_plus_tree_create_node(BPlusTree* tree, NodeType type);
void b_plus_tree_destroy_node(BPlusTree* tree, BPlusTreeNode* node);
bool b_plus_tree_node_is_full(BPlusTreeNode* node);
bool b_plus_tree_node_is_underflow(BPlusTreeNode* node);
BPlusTreeNode* b_plus_tree_split_node(BPlusTree* tree, BPlusTreeNode* node);
BPlusTreeNode* b_plus_tree_merge_nodes(BPlusTree* tree, BPlusTreeNode* left, BPlusTreeNode* right);
BPlusTreeNode* b_plus_tree_find_leaf(BPlusTree* tree, void* key);
int b_plus_tree_find_key_index(BPlusTreeNode* node, void* key, int (*compare)(const void*, const void*));

#endif // B_PLUS_TREE_H