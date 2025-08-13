#ifndef TREE_H
#define TREE_H
#include <stdbool.h>
#define BTREE_T 2
typedef struct BTreeNode {
    int n;
    char (*keys)[256];
    long *positions;
    struct BTreeNode **C;
    bool leaf;
} BTreeNode;
BTreeNode* btree_create();
void btree_insert(BTreeNode** root, const char* key, long position);
long btree_search(BTreeNode* root, const char* key);
void btree_delete(BTreeNode** root, const char* key);
void btree_list(BTreeNode* root);
#endif
