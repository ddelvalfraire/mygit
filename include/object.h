#ifndef OBJECT_H
#define OBJECT_H

#define HEX_SIZE (SHA256_SIZE * 2 + 1)
#define SHA256_SIZE 32
#define FILE_MAX 4096

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
} object_data_t;


typedef struct object object_t;

object_t *object_init(object_type_t type);
void object_free(object_t *obj);

int object_update(object_t *obj, object_data_t data);
int object_write(object_t *obj, char *out_hash);



#endif // OBJECT_H