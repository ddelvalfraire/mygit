#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

node_t *createNode(char *name, int isFile)
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

    node->is_file = isFile;
    node->child_count = 0;
    return node;
}

void addPath(node_t *root, char *path, void* data)
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
            node_t *newNode = createNode(token, 0);
            if (!newNode)
            {
                free(pathCopy);
                return;
            }
            current->children[current->child_count++] = newNode;
            current = newNode;
        }
    }
    current->is_file = 1;
    if (data) current->data = data;
    free(pathCopy);
}

void printTree(node_t *node, int depth)
{
    if (!node)
        return;

    for (int i = 0; i < depth; i++)
        printf("  ");
    printf("%s%s\n", node->name, node->is_file ? " [File]" : "");

    for (int i = 0; i < node->child_count; i++)
    {
        printTree(node->children[i], depth + 1);
    }
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