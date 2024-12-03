#ifndef OBJECT_H
#define OBJECT_H

#include "staging.h"

#include <time.h>

typedef enum
{
     OBJ_BLOB,
     OBJ_TREE,
     OBJ_COMMIT,
     OBJ_TAG
} object_type_t;

typedef union
{
     struct
     {
          const char *filepath;
     } blob;
     struct
     {
          index_t *index;
     } tree;
     struct
     {
          const char *tree_hash;
          const char *parent_hash;
          const char *message;
          const char *author_name;
          const char *author_email;
          time_t author_time;
          const char *committer_name;
          const char *committer_email;
          time_t committer_time;
     } commit;
} object_update_t;

typedef struct object object_t;

object_t *object_init(object_type_t type);
void object_free(object_t *obj);

int object_update(object_t *obj, object_update_t data);
int object_write(object_t *obj, char *out_hash);
int object_read(object_t *obj, const char *hash);

#endif // OBJECT_H