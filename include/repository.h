

#ifndef REPOSITORY_H
#define REPOSITORY_H

typedef struct repository repository_t;

repository_t *repository_init();
void repository_free(repository_t *repo);

repository_t *repository_open();
int repository_add(repository_t *repo, int size, char **files);
int repository_commit(repository_t *repo, const char *message);
int repository_status(repository_t *repo);

#endif // REPOSITORY_H