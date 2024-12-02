

#ifndef REPOSITORY_H
#define REPOSITORY_H

typedef struct repository repository_t;

repository_t *repository_init(const char *path);
void repository_free(repository_t *repo);

int repository_destroy(repository_t *repo);

#endif // REPOSITORY_H