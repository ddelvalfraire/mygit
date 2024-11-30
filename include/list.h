// list.h
#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

#define LIST_INIT_CAPACITY 10

typedef enum
{
    LIST_OK,
    LIST_ERROR_NULL_INPUT,
    LIST_ERROR_OUT_OF_BOUNDS,
    LIST_ERROR_MEMORY_ALLOCATION_FAILED,
} list_error_t;

typedef struct
{
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
} list_t;

// Public API declarations
list_error_t list_init(list_t *list, size_t element_size);
list_error_t list_add(list_t *list, void *element);
list_error_t list_get(list_t *list, size_t index, void *element);
void list_free(list_t *list);
const char *list_error_to_string(list_error_t error);

#endif // LIST_H