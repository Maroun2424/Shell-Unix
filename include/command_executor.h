#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

extern int if_test_mode;
extern char g_forValue[]; 

int process_command(const char *input);

#endif // COMMAND_EXECUTOR_H
