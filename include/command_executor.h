#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

extern int last_exit_status;
extern int if_test_mode;
void process_command(const char *input);

#endif // COMMAND_EXECUTOR_H
