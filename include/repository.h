

#ifndef REPOSITORY_H
#define REPOSITORY_H

typedef struct repository repository_t;

repository_t *repository_init();
void repository_free(repository_t *repo);

repository_t *repository_open();
int repository_add(repository_t *repo, int size, char **files);

#endif // REPOSITORY_H