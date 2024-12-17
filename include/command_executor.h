#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

extern int last_exit_status;
void process_command(const char *input);
void process_sequential_commands(const char *input);

#endif // COMMAND_EXECUTOR_H
