#ifndef COMMANDS_H
#define COMMANDS_H

int command_init_validate(struct command *self, int argc, char **argv);
int command_init_run(struct command *self, int argc, char **argv);
void command_init_cleanup(struct command *self);

#endif // COMMANDS_H