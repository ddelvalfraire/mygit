#ifndef COMMAND_H
#define COMMAND_H

typedef struct command command_t;

command_t *command_init();
command_t *command_add();
command_t *command_commit();
command_t *command_status();
// command_t *log(void);

// Advanced commands (maybe implement later)

// command_t *checkout(void);
// command_t *branch(void);
// command_t *merge(void);
// command_t *tag(void);
// command_t *diff(void);
// command_t *clone(void);
// command_t *fetch(void);
// command_t *pull(void);
// command_t *push(void);
// command_t *reset(void);
// command_t *rebase(void);
// command_t *stash(void);

int command_execute(command_t *cmd, int argc, char **argv);

#endif // COMMAND_H