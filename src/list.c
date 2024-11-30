// list.c
#include <string.h>
#include "list.h"

static list_error_t _resize(list_t *list)
{
    list->capacity *= 2;
    list->data = realloc(list->data, list->capacity * list->element_size);

    if (!list->data)
    {
        return LIST_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    return LIST_OK;
}

list_error_t list_init(list_t *list, size_t element_size)
{
    if (!list)
    {
        return LIST_ERROR_NULL_INPUT;
    }

    list->element_size = element_size;
    list->size = 0;
    list->capacity = LIST_INIT_CAPACITY;
    list->data = malloc(list->capacity * element_size);

    if (!list->data)
    {
        return LIST_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    return LIST_OK;
}

list_error_t list_add(list_t *list, void *element)
{
    if (!list || !element)
    {
        return LIST_ERROR_NULL_INPUT;
    }

    if (list->size == list->capacity)
    {
        list_error_t err = _resize(list);
        if (err != LIST_OK)
        {
            return err;
        }
    }

    void *dest = (char *)list->data + list->size * list->element_size;
    memcpy(dest, element, list->element_size);
    list->size++;

    return LIST_OK;
}

list_error_t list_get(list_t *list, size_t index, void *element)
{
    if (!list || !element)
    {
        return LIST_ERROR_NULL_INPUT;
    }

    if (index >= list->size)
    {
        return LIST_ERROR_OUT_OF_BOUNDS;
    }

    void *src = (char *)list->data + index * list->element_size;
    memcpy(element, src, list->element_size);

    return LIST_OK;
}

void list_free(list_t *list)
{
    if (!list || !list->data)
    {
        return;
    }
    free(list->data);
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

const char *list_error_to_string(list_error_t error)
{
    switch (error)
    {
    case LIST_OK:
        return "No error";
    case LIST_ERROR_NULL_INPUT:
        return "Null input";
    case LIST_ERROR_OUT_OF_BOUNDS:
        return "Out of bounds";
    case LIST_ERROR_MEMORY_ALLOCATION_FAILED:
        return "Memory allocation failed";
    default:
        return "Unknown error";
    }
}