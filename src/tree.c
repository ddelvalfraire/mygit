#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

node_t *createNode(char *name)
{
    node_t *node = malloc(sizeof(node_t));
    if (!node)
        return NULL;

    node->name = strdup(name);
    if (!node->name)
    {
        free(node);
        return NULL;
    }

    node->is_file = 0;
    node->child_count = 0;
    return node;
}
void addPath(node_t *root, char *path, void *data)
{
    if (!root || !path)
        return;

    char *pathCopy = strdup(path);
    if (!pathCopy)
        return;

    char *token;
    char *rest = pathCopy;
    node_t *current = root;

    while ((token = strtok_r(rest, "/", &rest)))
    {
        int found = 0;
        for (int i = 0; i < current->child_count; i++)
        {
            if (strcmp(current->children[i]->name, token) == 0)
            {
                current = current->children[i];
                found = 1;
                break;
            }
        }
        if (!found)
        {
            if (current->child_count >= MAX_CHILDREN)
            {
                free(pathCopy);
                return;
            }
            node_t *newNode = createNode(token); 
            if (!newNode)
            {
                free(pathCopy);
                return;
            }
            current->children[current->child_count++] = newNode;
            current = newNode;
        }
    }

    // Last node becomes a file when data is assigned
    if (data) {
        current->data = data;
        current->is_file = 1;
    }

    free(pathCopy);
}
void freeTree(node_t *node)
{
    if (!node)
        return;

    for (int i = 0; i < node->child_count; i++)
    {
        freeTree(node->children[i]);
    }

    free(node->name);
    free(node);
}