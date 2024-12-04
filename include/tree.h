#ifndef TREE_H
#define TREE_H

#include "config.h"

typedef struct node_t
{
    char *name;
    void* data;
    int is_file;
    struct node_t *children[MAX_CHILDREN];
    int child_count;
} node_t;

// Function prototypes
node_t *createNode(char *name);
void addPath(node_t *root, char *path, void* data);
void printTree(node_t *node, int depth);
void freeTree(node_t *node); // Added memory cleanup function

#endif // TREE_H