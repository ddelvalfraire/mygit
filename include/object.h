#ifndef OBJECT_H
#define OBJECT_H

#include "staging.h"



typedef enum
{
    OBJ_BLOB,
    OBJ_TREE,
    OBJ_COMMIT,
    OBJ_TAG
} object_type_t;

typedef union {
   struct {
        const char *filepath;
   } blob;
   struct {
        index_t *index;
   } tree;
} object_data_t;


typedef struct object object_t;

object_t *object_init(object_type_t type);
void object_free(object_t *obj);

int object_update(object_t *obj, object_data_t data);
int object_write(object_t *obj, char *out_hash);



#endif // OBJECT_H